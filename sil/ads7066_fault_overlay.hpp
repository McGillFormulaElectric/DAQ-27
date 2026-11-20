#pragma once

#include "ads7066_model.hpp"

#include <cstdint>
#include <deque>

namespace sil::ads7066 {

enum class FaultKind {
    PowerReset,
    Brownout,
    GpioLevel,
    ConversionCode,
    DelayReadiness,
    CorruptInputBit,
    FlipSdoBit,
    ForceSdoLow,
    ForceSdoHigh,
    ReleaseSdo
};

struct FaultEvent {
    uint64_t at_ns{};
    FaultKind kind{};
    uint8_t index{};
    uint64_t value{};
};

class Ads7066FaultOverlay {
public:
    explicit Ads7066FaultOverlay(Ads7066Model& device) : device_(device) {}
    void schedule(FaultEvent event);
    void set_cs(bool high);
    PinLevel clock_spi_bit(bool sdi);
    PinLevel clock_edge(bool sclk_high, bool sdi);
    void advance_time(uint64_t duration_ns);
    std::size_t pending_events() const { return events_.size(); }
    static unsigned precedence(FaultKind kind);

private:
    Ads7066Model& device_;
    std::deque<FaultEvent> events_;
    bool flip_next_{};
    bool corrupt_next_input_{};
    PinLevel forced_{PinLevel::HighZ};
    uint64_t armed_readiness_delay_ns_{};
    bool cs_high_{true};
    void apply_due();
};

} // namespace sil::ads7066
