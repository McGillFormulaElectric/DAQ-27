#pragma once

#include "ads7066_model.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace sil::ads7066 {

enum class BusResult { Ok, NoDeviceSelected, Contention, InvalidDaisyFrame, DaisyPassThroughDisabled };

struct BusTransfer {
    BusResult result{BusResult::Ok};
    std::vector<uint8_t> miso;
};

class Ads7066Bus {
public:
    explicit Ads7066Bus(std::vector<Ads7066Model*> devices);
    void set_cs(std::size_t device, bool high);
    bool cs_high(std::size_t device) const;
    BusTransfer transfer(const std::vector<uint8_t>& mosi);
    void advance_time(uint64_t duration_ns);

private:
    std::vector<Ads7066Model*> devices_;
    std::vector<bool> cs_high_;
};

// Explicit continuous host -> first -> ... -> last topology. Per SBAS928C
// 7.3.10.2, SDI propagates through each device with a 24-SCLK delay.
class Ads7066DaisyChain {
public:
    explicit Ads7066DaisyChain(std::vector<Ads7066Model*> host_to_miso_order);
    ~Ads7066DaisyChain();
    Ads7066DaisyChain(const Ads7066DaisyChain&)=delete;
    Ads7066DaisyChain& operator=(const Ads7066DaisyChain&)=delete;
    Ads7066DaisyChain(Ads7066DaisyChain&&)=delete;
    Ads7066DaisyChain& operator=(Ads7066DaisyChain&&)=delete;
    void set_cs(bool high);
    BusTransfer transfer(const std::vector<uint8_t>& host_mosi);
    bool enabled() const;
    void advance_time(uint64_t duration_ns);

private:
    std::vector<Ads7066Model*> devices_;
    bool cs_high_{true};
};

} // namespace sil::ads7066
