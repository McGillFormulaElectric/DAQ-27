#include "ads7066_model.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace sil::ads7066 {
namespace {
constexpr uint64_t kEdgeResolutionNs = 10;       // arithmetic resolution only
constexpr uint64_t kConversionMaxNs = 3200;      // SBAS928C 6.7 tCONV maximum
constexpr uint64_t kPowerResetMaxNs = 5000000;   // SBAS928C 6.7 tPU/tRST maximum

std::vector<bool> bytes_to_bits(const std::vector<uint8_t>& bytes) {
    std::vector<bool> bits; bits.reserve(bytes.size() * 8u);
    for (uint8_t byte : bytes) for (int bit = 7; bit >= 0; --bit) bits.push_back((byte & (1u << bit)) != 0u);
    return bits;
}
std::vector<uint8_t> bits_to_bytes(const std::vector<bool>& bits) {
    std::vector<uint8_t> bytes((bits.size() + 7u) / 8u,0);
    for (std::size_t i = 0; i < bits.size(); ++i) if (bits[i]) bytes[i / 8u] |= static_cast<uint8_t>(1u << (7u - i % 8u));
    return bytes;
}
}

const std::array<RegisterRule,17>& Ads7066Model::register_rules() {
    static const std::array<RegisterRule,17> rules{{
        {0x00,0x81,0x03,0x03,false},{0x01,0x00,0xCF,0,false},{0x02,0x00,0xB3,0,false},
        {0x03,0x00,0x07,0,false},{0x04,0x04,0x1F,0,false},{0x05,0x00,0xFF,0,false},
        {0x07,0x00,0xFF,0,false},{0x09,0x00,0xFF,0,false},{0x0B,0x00,0xFF,0,false},
        {0x0D,0x00,0x00,0,false},{0x10,0x00,0x13,0,false},{0x11,0x00,0x0F,0,false},
        {0x12,0x00,0xFF,0,false},{0xBF,0x00,0xFF,0,false},{0xC0,0x00,0x11,0,true},
        {0xC1,0x00,0xFF,0,true},{0xC2,0x00,0xFF,0,true}
    }};
    return rules;
}

Ads7066Model::Ads7066Model() { supply_on_ = false; power_on(); }

void Ads7066Model::reset_state(bool bor) {
    registers_.fill(0); for (const auto& rule : register_rules()) registers_[rule.address] = rule.reset;
    if (!bor) registers_[0] &= static_cast<uint8_t>(~1u);
    pending_read_ = false; pending_register_ = 0; input_bits_.clear(); output_bits_.clear(); output_index_ = 0;
    reserved_accesses_.clear(); invalid_frame_clocks_.clear(); diagnostic_unlocked_ = false;
    calibration_done_ns_ = 0; pending_conversion_ = {}; premature_reads_ = 0;
    manual_channel_ = 0; sequence_channel_ = 0; sequence_active_ = false;
    completed_channel_ = 0; completed_code_ = analog_codes_[0] & 0xFFFFu; conversion_valid_ = true;
}

void Ads7066Model::power_on() {
    if (!supply_on_) { supply_on_ = true; reset_state(true); power_ready_ns_ = now_ns_ + kPowerResetMaxNs; }
}
void Ads7066Model::power_off() { supply_on_ = false; input_bits_.clear(); output_bits_.clear(); output_index_ = 0; output_launched_ = false; }
void Ads7066Model::brownout() { if (supply_on_) { reset_state(true); power_ready_ns_ = now_ns_ + kPowerResetMaxNs; } }

uint8_t Ads7066Model::crc8(const uint8_t* data,std::size_t size) {
    uint8_t crc = 0xFF;
    for (std::size_t n = 0; n < size; ++n) { crc ^= data[n]; for (int bit = 0; bit < 8; ++bit) crc = static_cast<uint8_t>((crc & 0x80u) ? ((crc << 1u) ^ 0x07u) : (crc << 1u)); }
    return crc;
}

void Ads7066Model::set_cs(bool high) {
    run_scheduled_events();
    if (!responsive()) { cs_high_ = high; return; }
    if (high == cs_high_) return;
    if (!high) {
        input_bits_.clear(); output_index_ = 0; prepare_output();
        last_sclk_ = (registers_[2] & 0x02u) != 0u;
        output_launched_ = (registers_[2] & 0x01u) == 0u;
    } else {
        start_conversion_from_edge(); // sampling precedes command activation on this edge
        decode_frame();
        if ((registers_[0x10] & 3u) == 0u) { const uint8_t requested=registers_[0x11]&0x0Fu; if(requested<8u)manual_channel_=requested; }
    }
    cs_high_ = high;
}

PinLevel Ads7066Model::sdo() const {
    if (!responsive() || cs_high_) return PinLevel::HighZ;
    if (!output_launched_) return PinLevel::Low;
    if (daisy_chain_active_ && daisy_pass_through_enabled() && output_index_ >= 24u) {
        const std::size_t pass_index = output_index_ - 24u;
        if (pass_index < input_bits_.size()) return input_bits_[pass_index] ? PinLevel::High : PinLevel::Low;
    }
    if (output_index_ < output_bits_.size()) return output_bits_[output_index_] ? PinLevel::High : PinLevel::Low;
    return PinLevel::Low;
}

PinLevel Ads7066Model::clock_spi_bit(bool sdi) {
    run_scheduled_events(); if (!responsive() || cs_high_) return PinLevel::HighZ;
    output_launched_ = true;
    const PinLevel out = sdo(); input_bits_.push_back(sdi); ++output_index_;
    now_ns_ += 2u * kEdgeResolutionNs; run_scheduled_events(); return out;
}

PinLevel Ads7066Model::clock_edge(bool sclk_high,bool sdi) {
    now_ns_ += kEdgeResolutionNs; run_scheduled_events();
    if (!responsive() || cs_high_) { last_sclk_ = sclk_high; return PinLevel::HighZ; }
    if (sclk_high == last_sclk_) return sdo();
    const bool cpol = (registers_[2] & 2u) != 0u; const bool cpha = (registers_[2] & 1u) != 0u;
    const bool leading = sclk_high != cpol; last_sclk_ = sclk_high;
    const bool capture = leading != cpha;
    if (capture) { const PinLevel out = sdo(); input_bits_.push_back(sdi); return out; }
    if (cpha && !output_launched_) output_launched_ = true;
    else ++output_index_;
    return sdo();
}

void Ads7066Model::advance_time(uint64_t duration_ns) { now_ns_ += duration_ns; run_scheduled_events(); }
void Ads7066Model::advance_to_external_event(uint64_t absolute_ns) {
    if(absolute_ns<now_ns_)throw std::invalid_argument("ADS7066 external event is in the past");
    if(absolute_ns>now_ns_){now_ns_=absolute_ns-1u;run_scheduled_events();now_ns_=absolute_ns;}
}
void Ads7066Model::set_analog_code(uint8_t channel,uint32_t code) { if (channel >= 8) throw std::out_of_range("ADS7066 analog channel"); analog_codes_[channel] = code & 0xFFFFu; }
void Ads7066Model::queue_analog_code(uint8_t channel,uint32_t code,uint64_t available_ns) {
    if (channel >= 8) throw std::out_of_range("ADS7066 analog channel");
    auto& queue=analog_queues_[channel]; const TimedAnalogCode entry{available_ns,code & 0xFFFFu};
    queue.insert(std::upper_bound(queue.begin(),queue.end(),entry,[](const TimedAnalogCode& a,const TimedAnalogCode& b){return a.available_ns < b.available_ns;}),entry);
}
void Ads7066Model::set_gpio_input(uint8_t pin,bool level) { if (pin >= 8) throw std::out_of_range("ADS7066 GPIO"); gpio_inputs_[pin] = level; }

PinLevel Ads7066Model::gpio_drive(uint8_t pin) const {
    if (pin >= 8) throw std::out_of_range("ADS7066 GPIO");
    const uint8_t bit = static_cast<uint8_t>(1u << pin);
    if ((registers_[1] & 4u) != 0u || (registers_[5] & bit) == 0u || (registers_[7] & bit) == 0u) return PinLevel::HighZ;
    const bool high = (registers_[0x0B] & bit) != 0u; const bool push_pull = (registers_[9] & bit) != 0u;
    return high ? (push_pull ? PinLevel::High : PinLevel::HighZ) : PinLevel::Low;
}

uint8_t Ads7066Model::read_register(uint8_t address) const {
    if (address == 0x0D) {
        if ((registers_[1] & 4u) != 0u) return 0;
        uint8_t value = 0; for (uint8_t pin = 0; pin < 8; ++pin) { const uint8_t bit = static_cast<uint8_t>(1u << pin); if ((registers_[5] & bit) != 0u && (registers_[7] & bit) == 0u && gpio_inputs_[pin]) value |= bit; }
        return value;
    }
    return registers_[address];
}

void Ads7066Model::prepare_output() {
    std::vector<uint8_t> bytes;
    if (pending_read_) {
        const uint8_t value = read_register(pending_register_);
        bytes = (registers_[1] & 0x40u) ? std::vector<uint8_t>{value,crc8(&value,1),0,0} : std::vector<uint8_t>{value,0,0};
        pending_read_ = false;
    } else { if (pending_conversion_.active) ++premature_reads_; bytes = conversion_payload(); }
    output_bits_ = bytes_to_bits(bytes);
}

void Ads7066Model::decode_frame() {
    if (input_bits_.empty()) return;
    const bool crc_enabled = (registers_[1] & 0x40u) != 0u; const std::size_t required = crc_enabled ? 32u : 24u;
    if (!crc_enabled && (input_bits_.size() == 16u || input_bits_.size() == 20u)) return; // documented conversion/on-the-fly read lengths
    if ((!daisy_chain_active_ && input_bits_.size() != required) || (daisy_chain_active_ && input_bits_.size() < required)) { invalid_frame_clocks_.push_back(static_cast<uint16_t>(input_bits_.size())); return; }
    const std::vector<bool> command_bits(input_bits_.end() - static_cast<std::ptrdiff_t>(required),input_bits_.end()); const auto bytes = bits_to_bytes(command_bits);
    if (crc_enabled && crc8(bytes.data(),3) != bytes[3]) { registers_[0] |= 2u; return; }
    execute_command(bytes[0],bytes[1],bytes[2]);
}

void Ads7066Model::execute_command(uint8_t opcode,uint8_t address,uint8_t value) {
    if (opcode == 0x00) return;
    if (opcode == 0x10) { pending_register_ = address; pending_read_ = true; return; }
    if (opcode != 0x08 && opcode != 0x18 && opcode != 0x20) { reserved_accesses_.push_back({opcode,address,value}); return; }
    write_register(opcode,address,value);
}

void Ads7066Model::write_register(uint8_t opcode,uint8_t address,uint8_t value) {
    const auto it = std::find_if(register_rules().begin(),register_rules().end(),[address](const RegisterRule& r){return r.address == address;});
    if (it == register_rules().end() || it->writable_mask == 0u || (it->diagnostic_lock && !diagnostic_unlocked_)) { reserved_accesses_.push_back({opcode,address,value}); return; }
    if ((registers_[0] & 2u) != 0u && address != 0u && address != 1u) return;
    const uint8_t masked = value & it->writable_mask;
    if (it->w1c_mask) registers_[address] &= static_cast<uint8_t>(~(masked & it->w1c_mask));
    else if (opcode == 0x08) registers_[address] = static_cast<uint8_t>((registers_[address] & ~it->writable_mask) | masked);
    else if (opcode == 0x18) registers_[address] |= masked;
    else registers_[address] &= static_cast<uint8_t>(~masked);
    if (address == 0xBF && registers_[0xBF] == 0x96u) diagnostic_unlocked_ = true; // no undocumented relock rule
    if (address == 1 && (masked & 1u) != 0u) { registers_[1] &= static_cast<uint8_t>(~1u); reset_due_ns_ = now_ns_ + kPowerResetMaxNs; }
    if (address == 1 && (registers_[1] & 2u) != 0u && calibration_duration_ns_ != 0u) calibration_done_ns_ = now_ns_ + calibration_duration_ns_;
    refresh_derived_state();
}

void Ads7066Model::refresh_derived_state() {
    const bool active = (registers_[0x10] & 0x13u) == 0x11u && registers_[0x12] != 0u;
    if (active && (!sequence_active_ || (registers_[0x12] & (1u << sequence_channel_)) == 0u)) sequence_channel_ = next_enabled(registers_[0x12],7);
    sequence_active_ = active; if (active) registers_[0] |= 0x40u; else registers_[0] &= static_cast<uint8_t>(~0x40u);
}

uint32_t Ads7066Model::sample_code_at(uint8_t channel,uint64_t sample_ns) {
    auto& queue = analog_queues_[channel]; while (!queue.empty() && queue.front().available_ns <= sample_ns) { analog_codes_[channel] = queue.front().code; queue.pop_front(); }
    return analog_codes_[channel] & 0xFFFFu;
}

void Ads7066Model::start_conversion_from_edge() {
    uint8_t channel = manual_channel_; const uint8_t mode = registers_[0x10] & 3u;
    if (mode == 1u && sequence_active_) channel = sequence_channel_;
    else if (mode == 2u && input_bits_.size() >= 5u && input_bits_[0]) { uint8_t requested = 0; for (std::size_t i = 1; i < 5; ++i) requested = static_cast<uint8_t>((requested << 1u) | (input_bits_[i] ? 1u : 0u)); if (requested < 8) channel = requested; }
    if (channel >= 8) return;
    const uint8_t bit = static_cast<uint8_t>(1u << channel);
    pending_conversion_ = {}; pending_conversion_.active = true; pending_conversion_.channel = channel;
    pending_conversion_.valid = ((registers_[5] & bit) == 0u || (registers_[1] & 4u) != 0u);
    pending_conversion_.sample_count = 1u << (registers_[3] & 7u); pending_conversion_.samples_taken = 1;
    if ((registers_[0xC0] & 1u) != 0u) { pending_conversion_.diagnostic_source = true; pending_conversion_.diagnostic_code = (static_cast<uint32_t>(registers_[0xC2]) << 8u) | registers_[0xC1]; }
    else if ((registers_[0xC0] & 0x10u) != 0u && channel == 6u) { pending_conversion_.diagnostic_source = true; pending_conversion_.diagnostic_code = vtest_code_; }
    pending_conversion_.sum = pending_conversion_.diagnostic_source ? pending_conversion_.diagnostic_code : sample_code_at(channel,now_ns_);
    const uint64_t cycle = cycle_time_ns(registers_[4]); pending_conversion_.next_sample_ns = now_ns_ + cycle;
    pending_conversion_.ready_ns = pending_conversion_.sample_count == 1u ? now_ns_ + kConversionMaxNs : now_ns_ + cycle * pending_conversion_.sample_count * 106u / 100u;
    if (mode == 1u && sequence_active_) sequence_channel_ = next_enabled(registers_[0x12],channel);
}

uint8_t Ads7066Model::status_flags() const { const bool vtest = (registers_[0xC0] & 0x10u) && completed_channel_ == 6u && !(registers_[0xC0] & 1u); return static_cast<uint8_t>(8u | (vtest ? 4u : 0u) | ((registers_[0] & 2u) ? 2u : 0u) | ((registers_[0xC0] & 1u) ? 1u : 0u)); }

std::vector<uint8_t> Ads7066Model::conversion_payload() const {
    const bool averaging = (registers_[3] & 7u) != 0u; const uint8_t append = static_cast<uint8_t>((registers_[2] >> 4u) & 3u);
    uint32_t raw = (registers_[2] & 0x80u) ? 0xA5A5u : completed_code_; if (!conversion_valid_) raw = 0;
    const uint8_t tail = append == 1u ? completed_channel_ : (append == 2u ? status_flags() : 0u); std::vector<uint8_t> bytes;
    if (averaging) { const uint32_t frame = ((raw & 0xFFFFFu) << 4u) | tail; bytes = {static_cast<uint8_t>(frame >> 16u),static_cast<uint8_t>(frame >> 8u),static_cast<uint8_t>(frame)}; }
    else bytes = {static_cast<uint8_t>(raw >> 8u),static_cast<uint8_t>(raw),static_cast<uint8_t>(tail << 4u)};
    if (registers_[2] & 0x80u) bytes = {0xA5,0xA5,0};
    if (registers_[1] & 0x40u) { const std::size_t payload = (!averaging && (append == 0u || append == 3u || (registers_[2] & 0x80u))) ? 2u : 3u; const uint8_t crc = crc8(bytes.data(),payload); if (payload == 2u) bytes = {bytes[0],bytes[1],crc,0}; else bytes.push_back(crc); }
    return bytes;
}

void Ads7066Model::run_scheduled_events() {
    if (reset_due_ns_ != 0u && now_ns_ >= reset_due_ns_) { reset_due_ns_ = 0; reset_state(true); power_ready_ns_ = now_ns_; }
    if (calibration_done_ns_ != 0u && now_ns_ >= calibration_done_ns_) { registers_[1] &= static_cast<uint8_t>(~2u); calibration_done_ns_ = 0; }
    if (!pending_conversion_.active) return;
    const uint64_t cycle = cycle_time_ns(registers_[4]);
    while (pending_conversion_.samples_taken < pending_conversion_.sample_count && pending_conversion_.next_sample_ns <= now_ns_) {
        pending_conversion_.sum += pending_conversion_.diagnostic_source ? pending_conversion_.diagnostic_code : sample_code_at(pending_conversion_.channel,pending_conversion_.next_sample_ns);
        ++pending_conversion_.samples_taken; pending_conversion_.next_sample_ns += cycle;
    }
    if (now_ns_ >= pending_conversion_.ready_ns && pending_conversion_.samples_taken == pending_conversion_.sample_count) {
        completed_channel_ = pending_conversion_.channel; conversion_valid_ = pending_conversion_.valid;
        completed_code_ = pending_conversion_.sample_count == 1u ? static_cast<uint32_t>(pending_conversion_.sum) : static_cast<uint32_t>((pending_conversion_.sum << 4u) / pending_conversion_.sample_count) & 0xFFFFFu;
        pending_conversion_.active = false;
    }
}

uint8_t Ads7066Model::next_enabled(uint8_t mask,uint8_t after) { for (unsigned offset = 1; offset <= 8; ++offset) { const uint8_t channel = static_cast<uint8_t>((after + offset) & 7u); if (mask & (1u << channel)) return channel; } return 0; }
uint64_t Ads7066Model::cycle_time_ns(uint8_t opmode) { static constexpr std::array<uint16_t,16> high{{0,0,0,0,4,6,8,12,16,24,32,48,64,96,128,192}}; static constexpr std::array<uint16_t,16> low{{32,48,64,96,128,192,256,384,512,768,1024,1536,2048,3072,4096,6144}}; const uint8_t d = opmode & 15u; return static_cast<uint64_t>((opmode & 0x10u) ? low[d] : high[d]) * 1000u; }
void Ads7066Model::delay_readiness(uint64_t duration_ns) { if (pending_conversion_.active) { if(duration_ns>std::numeric_limits<uint64_t>::max()-pending_conversion_.ready_ns)throw std::overflow_error("ADS7066 readiness deadline overflow"); pending_conversion_.ready_ns += duration_ns; } }
bool Ads7066Model::daisy_pass_through_enabled() const {
    const bool crc_enabled = (registers_[1] & 0x40u) != 0u;
    if (!crc_enabled) return true;
    const bool averaging = (registers_[3] & 7u) != 0u;
    const uint8_t append = static_cast<uint8_t>((registers_[2] >> 4u) & 3u);
    return !averaging && (append == 0u || append == 3u);
}

} // namespace sil::ads7066
