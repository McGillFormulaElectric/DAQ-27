#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <vector>

namespace sil::ads7066 {

class Ads7066FaultOverlay;

enum class PinLevel : uint8_t { Low = 0, High = 1, HighZ = 2 };

struct TimedAnalogCode {
    uint64_t available_ns{};
    uint32_t code{};
};

struct RegisterRule {
    uint8_t address{};
    uint8_t reset{};
    uint8_t writable_mask{};
    uint8_t w1c_mask{};
    bool diagnostic_lock{};
};

class Ads7066Model {
public:
    Ads7066Model();

    void power_on();
    void power_off();
    bool powered() const { return supply_on_; }
    bool responsive() const { return supply_on_ && now_ns_ >= power_ready_ns_ && reset_due_ns_ == 0u; }
    void set_cs(bool high);
    PinLevel clock_edge(bool sclk_high, bool sdi);
    PinLevel clock_spi_bit(bool sdi);
    PinLevel sdo() const;
    void advance_time(uint64_t duration_ns);
    // Advances to an external-event boundary after dispatching only internal
    // events strictly before it. The overlay applies external events there,
    // then calls dispatch_current_time() for equal-time internal events.
    void advance_to_external_event(uint64_t absolute_ns);
    void dispatch_current_time() { run_scheduled_events(); }
    uint64_t now_ns() const { return now_ns_; }

    void set_analog_code(uint8_t channel, uint32_t code);
    void queue_analog_code(uint8_t channel, uint32_t code, uint64_t available_ns);
    void set_gpio_input(uint8_t pin, bool level);
    void set_vtest_code(uint32_t code) { vtest_code_ = code & 0xFFFFu; }
    void set_calibration_duration(uint64_t duration_ns) { calibration_duration_ns_ = duration_ns; }
    void set_daisy_chain_active(bool active) { daisy_chain_active_ = active; }
    bool daisy_chain_active() const { return daisy_chain_active_; }
    void delay_readiness(uint64_t duration_ns);
    void brownout();
    bool daisy_pass_through_enabled() const;
    PinLevel gpio_drive(uint8_t pin) const;
    uint8_t read_register(uint8_t address) const;
    bool conversion_pending() const { return pending_conversion_.active; }
    uint64_t conversion_ready_ns() const { return pending_conversion_.ready_ns; }
    unsigned premature_reads() const { return premature_reads_; }
    const std::vector<uint16_t>& invalid_frames() const { return invalid_frame_clocks_; }
    const std::vector<std::array<uint8_t,3>>& reserved_accesses() const { return reserved_accesses_; }
    static const std::array<RegisterRule, 17>& register_rules();
    static uint8_t crc8(const uint8_t* data, std::size_t size);

private:
    struct PendingConversion {
        bool active{};
        bool valid{};
        uint8_t channel{};
        unsigned sample_count{};
        unsigned samples_taken{};
        uint64_t sum{};
        uint64_t next_sample_ns{};
        uint64_t ready_ns{};
        bool diagnostic_source{};
        uint32_t diagnostic_code{};
    };

    bool supply_on_{true};
    bool cs_high_{true};
    bool last_sclk_{};
    uint64_t now_ns_{};
    std::vector<bool> input_bits_;
    std::vector<bool> output_bits_;
    std::size_t output_index_{};
    bool output_launched_{};
    std::array<std::deque<TimedAnalogCode>, 8> analog_queues_{};
    std::array<uint32_t, 8> analog_codes_{};
    std::array<bool, 8> gpio_inputs_{};
    std::array<uint8_t,256> registers_{};
    std::vector<std::array<uint8_t,3>> reserved_accesses_;
    std::vector<uint16_t> invalid_frame_clocks_;
    bool pending_read_{};
    uint8_t pending_register_{};
    uint32_t completed_code_{};
    uint8_t completed_channel_{};
    bool conversion_valid_{true};
    bool diagnostic_unlocked_{};
    uint64_t calibration_done_ns_{};
    uint64_t calibration_duration_ns_{};
    uint64_t power_ready_ns_{};
    uint64_t reset_due_ns_{};
    bool daisy_chain_active_{};
    uint8_t manual_channel_{};
    uint8_t sequence_channel_{};
    bool sequence_active_{};
    unsigned premature_reads_{};
    uint32_t vtest_code_{};
    PendingConversion pending_conversion_{};

    void reset_state(bool bor);
    void prepare_output();
    void decode_frame();
    void execute_command(uint8_t opcode, uint8_t address, uint8_t value);
    void write_register(uint8_t opcode, uint8_t address, uint8_t value);
    void start_conversion_from_edge();
    void run_scheduled_events();
    void refresh_derived_state();
    uint32_t sample_code_at(uint8_t channel, uint64_t sample_ns);
    uint8_t status_flags() const;
    std::vector<uint8_t> conversion_payload() const;
    static uint8_t next_enabled(uint8_t mask, uint8_t after);
    static uint64_t cycle_time_ns(uint8_t opmode);
};

} // namespace sil::ads7066
