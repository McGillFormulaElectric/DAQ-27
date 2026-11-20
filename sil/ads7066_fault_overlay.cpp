#include "ads7066_fault_overlay.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace sil::ads7066 {

unsigned Ads7066FaultOverlay::precedence(FaultKind kind) {
    switch (kind) {
        case FaultKind::PowerReset: return 0;
        case FaultKind::Brownout: return 1;
        case FaultKind::DelayReadiness: return 2;
        case FaultKind::ConversionCode: return 3;
        case FaultKind::GpioLevel: return 4;
        case FaultKind::CorruptInputBit: return 5;
        case FaultKind::FlipSdoBit: return 6;
        case FaultKind::ForceSdoLow: case FaultKind::ForceSdoHigh: case FaultKind::ReleaseSdo: return 7;
    }
    return 7;
}

void Ads7066FaultOverlay::schedule(FaultEvent event) {
    if (event.at_ns < device_.now_ns()) throw std::invalid_argument("ADS7066 fault event is in the past");
    if ((event.kind == FaultKind::GpioLevel || event.kind == FaultKind::ConversionCode) && event.index >= 8u) throw std::out_of_range("ADS7066 fault channel");
    events_.push_back(event);
    std::stable_sort(events_.begin(),events_.end(),[](const FaultEvent& a,const FaultEvent& b){
        return a.at_ns < b.at_ns || (a.at_ns == b.at_ns && precedence(a.kind) < precedence(b.kind));
    });
}

void Ads7066FaultOverlay::apply_due() {
    while (!events_.empty() && events_.front().at_ns <= device_.now_ns()) {
        const auto event = events_.front(); events_.pop_front();
        switch (event.kind) {
            case FaultKind::PowerReset: device_.power_off(); device_.power_on(); break;
            case FaultKind::Brownout: device_.brownout(); break;
            case FaultKind::GpioLevel: device_.set_gpio_input(event.index, event.value != 0u); break;
            case FaultKind::ConversionCode: device_.set_analog_code(event.index,static_cast<uint32_t>(event.value)); break;
            case FaultKind::DelayReadiness:
                if(!cs_high_ && device_.conversion_pending()) device_.delay_readiness(event.value);
                else { if(event.value>std::numeric_limits<uint64_t>::max()-armed_readiness_delay_ns_)throw std::overflow_error("ADS7066 readiness delay overflow"); armed_readiness_delay_ns_+=event.value; }
                break;
            case FaultKind::CorruptInputBit: corrupt_next_input_ = true; break;
            case FaultKind::FlipSdoBit: flip_next_ = true; break;
            case FaultKind::ForceSdoLow: forced_ = PinLevel::Low; break;
            case FaultKind::ForceSdoHigh: forced_ = PinLevel::High; break;
            case FaultKind::ReleaseSdo: forced_ = PinLevel::HighZ; break;
        }
    }
}

void Ads7066FaultOverlay::set_cs(bool high) {
    apply_due(); device_.set_cs(high);
    if(high && armed_readiness_delay_ns_ != 0u && device_.conversion_pending()){device_.delay_readiness(armed_readiness_delay_ns_);armed_readiness_delay_ns_=0;}
    cs_high_=high;
}

PinLevel Ads7066FaultOverlay::clock_spi_bit(bool sdi) {
    apply_due(); if (corrupt_next_input_) { sdi = !sdi; corrupt_next_input_ = false; } PinLevel result = device_.clock_spi_bit(sdi);
    if (forced_ != PinLevel::HighZ) result = forced_;
    if (flip_next_ && result != PinLevel::HighZ) { result = result == PinLevel::High ? PinLevel::Low : PinLevel::High; flip_next_ = false; }
    return result;
}

PinLevel Ads7066FaultOverlay::clock_edge(bool sclk_high,bool sdi) {
    apply_due(); if (corrupt_next_input_) { sdi = !sdi; corrupt_next_input_ = false; } PinLevel result = device_.clock_edge(sclk_high,sdi);
    if (forced_ != PinLevel::HighZ) result = forced_;
    if (flip_next_ && result != PinLevel::HighZ) { result = result == PinLevel::High ? PinLevel::Low : PinLevel::High; flip_next_ = false; }
    return result;
}

void Ads7066FaultOverlay::advance_time(uint64_t duration_ns) {
    if(duration_ns>std::numeric_limits<uint64_t>::max()-device_.now_ns())throw std::overflow_error("ADS7066 fault time overflow");
    const uint64_t target=device_.now_ns()+duration_ns;
    while(!events_.empty()&&events_.front().at_ns<=target){
        device_.advance_to_external_event(events_.front().at_ns);
        apply_due();
        device_.dispatch_current_time();
    }
    device_.advance_time(target-device_.now_ns());
}

} // namespace sil::ads7066
