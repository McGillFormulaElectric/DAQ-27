#include "ads7066_bus.hpp"

#include <algorithm>
#include <stdexcept>
#include <unordered_set>

namespace sil::ads7066 {
namespace { constexpr uint64_t kSpiBitCellNs=20; }

Ads7066Bus::Ads7066Bus(std::vector<Ads7066Model*> devices)
    : devices_(std::move(devices)), cs_high_(devices_.size(), true) {
    if (std::any_of(devices_.begin(),devices_.end(),[](const auto* p){return p == nullptr;})) throw std::invalid_argument("null ADS7066 bus device");
    if (std::unordered_set<Ads7066Model*>(devices_.begin(),devices_.end()).size() != devices_.size()) throw std::invalid_argument("duplicate ADS7066 bus device");
}

void Ads7066Bus::set_cs(std::size_t device, bool high) {
    if (device >= devices_.size()) throw std::out_of_range("ADS7066 CS");
    cs_high_[device] = high; devices_[device]->set_cs(high);
}

bool Ads7066Bus::cs_high(std::size_t device) const {
    if (device >= devices_.size()) throw std::out_of_range("ADS7066 CS");
    return cs_high_[device];
}

BusTransfer Ads7066Bus::transfer(const std::vector<uint8_t>& mosi) {
    std::size_t selected = devices_.size(); unsigned count = 0;
    for (std::size_t i = 0; i < cs_high_.size(); ++i) if (!cs_high_[i]) { selected = i; ++count; }
    if (count == 0) { const uint64_t elapsed=static_cast<uint64_t>(mosi.size())*8u*kSpiBitCellNs; for(auto* d:devices_)d->advance_time(elapsed); return {BusResult::NoDeviceSelected,{}}; }
    if (count != 1) {
        // Both selected devices physically receive the clocks and SDI. MISO is
        // deliberately unresolved, but state transitions still occur.
        for(uint8_t byte:mosi)for(int bit=7;bit>=0;--bit)for(std::size_t i=0;i<devices_.size();++i){if(!cs_high_[i])(void)devices_[i]->clock_spi_bit((byte&(1u<<bit))!=0u);else devices_[i]->advance_time(kSpiBitCellNs);}
        return {BusResult::Contention,{}};
    }
    std::vector<uint8_t> miso; miso.reserve(mosi.size());
    for(uint8_t byte:mosi){uint8_t response=0;for(int bit=7;bit>=0;--bit){response=static_cast<uint8_t>(response<<1u);if(devices_[selected]->clock_spi_bit((byte&(1u<<bit))!=0u)==PinLevel::High)response|=1u;for(std::size_t i=0;i<devices_.size();++i)if(i!=selected)devices_[i]->advance_time(kSpiBitCellNs);}miso.push_back(response);}
    return {BusResult::Ok,std::move(miso)};
}

void Ads7066Bus::advance_time(uint64_t duration_ns) { for (auto* device : devices_) device->advance_time(duration_ns); }

Ads7066DaisyChain::Ads7066DaisyChain(std::vector<Ads7066Model*> host_to_miso_order)
    : devices_(std::move(host_to_miso_order)) {
    if (devices_.empty() || std::any_of(devices_.begin(),devices_.end(),[](const auto* p){return p == nullptr;})) throw std::invalid_argument("invalid ADS7066 daisy chain");
    if (std::unordered_set<Ads7066Model*>(devices_.begin(),devices_.end()).size() != devices_.size()) throw std::invalid_argument("duplicate ADS7066 daisy device");
    if (std::any_of(devices_.begin(),devices_.end(),[](const auto* p){return p->daisy_chain_active();})) throw std::invalid_argument("ADS7066 device already belongs to a daisy chain");
    for (auto* device : devices_) device->set_daisy_chain_active(true);
}

Ads7066DaisyChain::~Ads7066DaisyChain() { for (auto* device : devices_) device->set_daisy_chain_active(false); }

void Ads7066DaisyChain::set_cs(bool high) { cs_high_ = high; for (auto* device : devices_) device->set_cs(high); }

bool Ads7066DaisyChain::enabled() const {
    return std::all_of(devices_.begin(),devices_.end(),[](const Ads7066Model* d){return d->daisy_pass_through_enabled();});
}

BusTransfer Ads7066DaisyChain::transfer(const std::vector<uint8_t>& host_mosi) {
    if (cs_high_) return {BusResult::InvalidDaisyFrame,{}};
    const bool pass_through = enabled();
    std::vector<uint8_t> host_miso(host_mosi.size(),0);
    for (std::size_t byte = 0; byte < host_mosi.size(); ++byte) {
        for (int bit = 7; bit >= 0; --bit) {
            bool level = (host_mosi[byte] & (1u << bit)) != 0u;
            for (auto* device : devices_) level = device->clock_spi_bit(level) == PinLevel::High;
            if (level) host_miso[byte] |= static_cast<uint8_t>(1u << bit);
        }
    }
    return {pass_through ? BusResult::Ok : BusResult::DaisyPassThroughDisabled,std::move(host_miso)};
}

void Ads7066DaisyChain::advance_time(uint64_t duration_ns) { for (auto* device : devices_) device->advance_time(duration_ns); }

} // namespace sil::ads7066
