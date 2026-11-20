#pragma once
#include "fdcan.h"
#include "virtual_can.hpp"
#include <array>
#include <deque>
#include <string>
#include <vector>

struct SilCanCall {
    std::string name;
    FDCAN_TxHeaderTypeDef header{};
    std::array<uint8_t, 8> data{};
};
namespace silcan {
extern FDCAN_GlobalTypeDef regs;
extern FDCAN_HandleTypeDef handle;
extern std::vector<SilCanCall> calls;
extern FDCAN_ProtocolStatusTypeDef protocol;
extern FDCAN_ErrorCountersTypeDef counters;
extern sil::vcan::VirtualCanNode peer;
extern sil::vcan::VirtualCanBus bus;
void reset();
void script(HAL_StatusTypeDef);
void set_auto_ack(bool enabled);
void set_peer_mode(sil::vcan::NodeMode mode);
void set_frame_completion_latency_us(uint64_t delay_us);
void script_protocol_error(sil::vcan::ProtocolError error, unsigned attempts=1, uint64_t delay_us=0);
void set_retry_policy(unsigned max_attempts, uint64_t retry_delay_us);
void advance_us(uint64_t amount);
void force_terminal_buffer_for_test(unsigned buffer, bool transmitted);
int latest_buffer();
}
