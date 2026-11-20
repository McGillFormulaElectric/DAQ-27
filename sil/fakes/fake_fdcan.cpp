#include "fake_fdcan.hpp"
#include <algorithm>

namespace silcan {
FDCAN_GlobalTypeDef regs;
FDCAN_HandleTypeDef handle{&regs};
std::vector<SilCanCall> calls;
FDCAN_ProtocolStatusTypeDef protocol;
FDCAN_ErrorCountersTypeDef counters;
sil::vcan::VirtualCanNode peer;
sil::vcan::VirtualCanBus bus{peer};
static std::deque<HAL_StatusTypeDef> statuses;
static std::array<bool,3> occupied{};
static unsigned put_index=0;
static int latest=-1;
static unsigned max_attempts=1024;
static uint64_t retry_delay_us=1000;
static uint32_t lec(sil::vcan::ProtocolError error) {
    switch(error) {
        case sil::vcan::ProtocolError::Stuff:return 1; case sil::vcan::ProtocolError::Form:return 2;
        case sil::vcan::ProtocolError::Ack:return 3; case sil::vcan::ProtocolError::Bit1:return 4;
        case sil::vcan::ProtocolError::Bit0:return 5; case sil::vcan::ProtocolError::Crc:return 6;
        default:return 0;
    }
}
static void apply_events() {
    for(const auto& event:bus.take_controller_events()) {
        const unsigned buffer=event.token;
        if(buffer>=occupied.size()) continue;
        const uint32_t bit=1u<<buffer;
        if(event.kind==sil::vcan::ControllerEventKind::Acknowledged) {
            occupied[buffer]=false;regs.TXBRP&=~bit;regs.TXBTO|=bit;protocol.LastErrorCode=0;
        } else if(event.kind==sil::vcan::ControllerEventKind::AttemptError) {
            protocol.LastErrorCode=lec(event.error);
        }
    }
}
void reset() { regs = {}; calls.clear(); protocol = {}; counters = {}; statuses.clear(); occupied={}; put_index=0; latest=-1; max_attempts=1024;retry_delay_us=1000;bus.reset(); }
void script(HAL_StatusTypeDef s) { statuses.push_back(s); }
static HAL_StatusTypeDef next() { if (statuses.empty()) return HAL_OK; auto s=statuses.front(); statuses.pop_front(); return s; }
static HAL_StatusTypeDef log(const char* name) { calls.push_back({name,{},{}}); return next(); }
void set_auto_ack(bool enabled) { peer.set_mode(enabled?sil::vcan::NodeMode::Active:sil::vcan::NodeMode::ListenOnly); }
void set_peer_mode(sil::vcan::NodeMode mode) {peer.set_mode(mode);}
void set_frame_completion_latency_us(uint64_t delay_us) {peer.set_frame_completion_latency_us(delay_us);}
void script_protocol_error(sil::vcan::ProtocolError error,unsigned attempts,uint64_t delay_us){peer.script_error(error,attempts,delay_us);}
void set_retry_policy(unsigned attempts,uint64_t delay_us){max_attempts=attempts?attempts:1;retry_delay_us=delay_us;}
void advance_us(uint64_t amount){bus.advance_by(amount);apply_events();}
void force_terminal_buffer_for_test(unsigned buffer,bool acknowledged) {
    if(buffer>=occupied.size() || !occupied[buffer]) return;
    (void)bus.retire_request(buffer);
    const uint32_t bit=1u<<buffer;
    occupied[buffer]=false; regs.TXBRP&=~bit;
    if(acknowledged) regs.TXBTO|=bit;
    else {regs.TXBTO&=~bit;protocol.LastErrorCode=3;}
}
int latest_buffer() { return latest; }
}
extern "C" HAL_StatusTypeDef HAL_FDCAN_ConfigFilter(FDCAN_HandleTypeDef*, FDCAN_FilterTypeDef* f) {
    SilCanCall c{"filter",{}, {}}; c.header.Identifier=f->FilterID1; c.header.MessageMarker=f->FilterID2; silcan::calls.push_back(c); return silcan::next();
}
extern "C" HAL_StatusTypeDef HAL_FDCAN_ConfigGlobalFilter(FDCAN_HandleTypeDef*, uint32_t, uint32_t, uint32_t, uint32_t) { return silcan::log("global_filter"); }
extern "C" HAL_StatusTypeDef HAL_FDCAN_Start(FDCAN_HandleTypeDef*) { return silcan::log("start"); }
extern "C" HAL_StatusTypeDef HAL_FDCAN_Stop(FDCAN_HandleTypeDef*) { return silcan::log("stop"); }
extern "C" HAL_StatusTypeDef HAL_FDCAN_ActivateNotification(FDCAN_HandleTypeDef*, uint32_t, uint32_t) { return silcan::log("notify"); }
extern "C" HAL_StatusTypeDef HAL_FDCAN_AddMessageToTxFifoQ(FDCAN_HandleTypeDef*, FDCAN_TxHeaderTypeDef* h, uint8_t* d) {
    SilCanCall c{"tx",*h,{}}; std::copy(d,d+8,c.data.begin()); silcan::calls.push_back(c);
    const auto scripted=silcan::next();
    if(scripted!=HAL_OK) return scripted;
    int selected=-1;
    for(unsigned offset=0;offset<silcan::occupied.size();++offset) {
        const unsigned candidate=(silcan::put_index+offset)%silcan::occupied.size();
        if(!silcan::occupied[candidate]) {selected=static_cast<int>(candidate);break;}
    }
    if(selected<0) return HAL_BUSY;
    const unsigned buffer=static_cast<unsigned>(selected);
    silcan::occupied[buffer]=true;silcan::latest=selected;silcan::put_index=(buffer+1)%silcan::occupied.size();
    const uint32_t bit=1u<<buffer;silcan::regs.TXBTO&=~bit;silcan::regs.TXBRP|=bit;
    sil::vcan::Frame frame{};frame.identifier=h->Identifier;frame.length=static_cast<uint8_t>(std::min<uint32_t>(h->DataLength,8));
    std::copy(d,d+frame.length,frame.data.begin());
    if(!silcan::bus.queue_from_dut(buffer,frame,silcan::max_attempts,silcan::retry_delay_us)) {
        silcan::occupied[buffer]=false;silcan::regs.TXBRP&=~bit;return HAL_ERROR;
    }
    silcan::apply_events();
    return HAL_OK;
}
extern "C" HAL_StatusTypeDef HAL_FDCAN_GetProtocolStatus(FDCAN_HandleTypeDef*, FDCAN_ProtocolStatusTypeDef* s) { *s=silcan::protocol; return silcan::log("protocol"); }
extern "C" HAL_StatusTypeDef HAL_FDCAN_GetErrorCounters(FDCAN_HandleTypeDef*, FDCAN_ErrorCountersTypeDef* c) { *c=silcan::counters; return silcan::log("counters"); }
