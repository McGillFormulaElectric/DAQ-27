#include "CANDriver.h"
#include "fake_fdcan.hpp"
#include "sil_check.hpp"
#include <array>
#include <cstdlib>
#include <iostream>
#include <vector>

namespace {
int failures=0, known_defects=0;
void xfail_if(bool observed,const char* id,const char* defect){sil_check::xfail_if(observed,failures,known_defects,id,defect);}
CANMessage message(uint32_t id, uint8_t len=8) { const uint8_t d[8]{1,2,3,4,5,6,7,8}; return CANMessage(id,d,len); }

void sil_fdcan_001() {
    for (int fail=0; fail<4; ++fail) {
        silcan::reset(); CANDriver d{&silcan::handle,0,0x7ff};
        for(int i=0;i<fail;++i) silcan::script(HAL_OK);
        silcan::script(HAL_ERROR);
        CHECK(!d.initialize()); CHECK(silcan::calls.size()==static_cast<std::size_t>(fail+1));
    }
    silcan::reset(); CANDriver d{&silcan::handle,0,0x7ff}; CHECK(d.initialize());
    CHECK(silcan::calls.size()==4); CHECK(silcan::calls[0].name=="filter");
    CHECK(silcan::calls[0].header.Identifier==0); CHECK(silcan::calls[0].header.MessageMarker==0x7ff);
}
void sil_fdcan_002_003() {
    for(uint8_t len=0;len<=9;++len) {
        silcan::reset(); CANDriver d{&silcan::handle,0,0x7ff}; CHECK(d.configureTransmission());
        CHECK(d.addMessageToQueue(message(0x321,len))); CHECK(d.transmitMessage());
        const auto& h=silcan::calls.back().header; CHECK(h.Identifier==0x321); CHECK(h.IdType==FDCAN_STANDARD_ID);
        CHECK(h.TxFrameType==FDCAN_DATA_FRAME); CHECK(h.FDFormat==FDCAN_CLASSIC_CAN); CHECK(h.BitRateSwitch==FDCAN_BRS_OFF);
        CHECK(h.DataLength==(len>8?8:len)); CHECK(d.getQueueSize()==0);
    }
    silcan::reset(); CANDriver d{&silcan::handle,0,0x7ff}; d.configureTransmission();
    for(int i=0;i<CANDriver::MAX_QUEUE_CAPACITY;++i) CHECK(d.addMessageToQueue(message(static_cast<uint32_t>(i))));
    CHECK(!d.addMessageToQueue(message(99))); CHECK(d.getQueueSize()==CANDriver::MAX_QUEUE_CAPACITY);
    for (const auto status : {HAL_BUSY, HAL_ERROR, HAL_TIMEOUT}) {
        silcan::script(status); CHECK(!d.transmitMessage()); CHECK(d.getQueueSize()==CANDriver::MAX_QUEUE_CAPACITY);
    }
    CHECK(d.transmitMessage()); CHECK(d.getQueueSize()==CANDriver::MAX_QUEUE_CAPACITY-1);
    CHECK(d.addMessageToQueue(message(99))); // tail wraps to slot zero
    const std::size_t before_drain=silcan::calls.size();
    for(int i=1;i<CANDriver::MAX_QUEUE_CAPACITY;++i) CHECK(d.transmitMessage());
    CHECK(d.transmitMessage()); CHECK(d.isQueueEmpty());
    std::vector<uint32_t> drained;
    for(std::size_t i=before_drain;i<silcan::calls.size();++i) if(silcan::calls[i].name=="tx") drained.push_back(silcan::calls[i].header.Identifier);
    CHECK(drained.size()==50); for(uint32_t i=1;i<50;++i) CHECK(drained[i-1]==i); CHECK(drained.back()==99);
    const auto calls_before_priority=silcan::calls.size();
    const bool priority_stub=d.transmitPriorityMessage() && silcan::calls.size()==calls_before_priority;
    CHECK(priority_stub); xfail_if(priority_stub,"SIL-FDCAN-002","DEF-FDCAN-PRIORITY-STUB");
}
void sil_fdcan_004() {
    silcan::reset(); CANDriver d{&silcan::handle,0,0x7ff};
    silcan::protocol.BusOff=1; CHECK(!d.checkACK());
    CHECK(silcan::calls[silcan::calls.size()-2].name=="stop"); CHECK(silcan::calls.back().name=="start");
    silcan::reset(); silcan::regs.TXBRP=1; silcan::regs.TXBTO=0; CHECK(!d.checkACK());
    silcan::reset(); silcan::protocol.LastErrorCode=3; silcan::regs.TXBRP=0; silcan::regs.TXBTO=0;
    const bool unsafe_accepts_ack_error=d.checkACK();
    CHECK(unsafe_accepts_ack_error);

    // Two requests occupy distinct modeled hardware buffers. Completing the
    // first request is still reported false because checkACK scans the other
    // request's global pending bit instead of correlating the latest request.
    silcan::reset(); silcan::set_auto_ack(false); d.configureTransmission();
    CHECK(d.addMessageToQueue(message(0x101))); CHECK(d.addMessageToQueue(message(0x102)));
    CHECK(d.transmitMessage()); const int first=silcan::latest_buffer();
    CHECK(d.transmitMessage()); const int second=silcan::latest_buffer();
    CHECK(first==0 && second==1 && silcan::regs.TXBRP==0x3u);
    silcan::force_terminal_buffer_for_test(static_cast<unsigned>(first),true);
    CHECK((silcan::regs.TXBTO&(1u<<first))!=0u && (silcan::regs.TXBRP&(1u<<second))!=0u);
    const bool completed_request_misclassified=!d.checkACK();
    CHECK(completed_request_misclassified);
    silcan::force_terminal_buffer_for_test(static_cast<unsigned>(second),true); CHECK(d.checkACK());
    CHECK(d.addMessageToQueue(message(0x103))); CHECK(d.transmitMessage()); CHECK(silcan::latest_buffer()==2);
    silcan::force_terminal_buffer_for_test(2,true);
    CHECK(d.addMessageToQueue(message(0x104))); CHECK(d.transmitMessage()); CHECK(silcan::latest_buffer()==0);
    xfail_if(unsafe_accepts_ack_error || completed_request_misclassified,"SIL-FDCAN-004","DEF-FDCAN-ACK-CORRELATION");
    silcan::reset(); silcan::script(HAL_ERROR); silcan::script(HAL_ERROR);
    const bool ignores_hal_status=d.checkACK();
    CHECK(ignores_hal_status); xfail_if(ignores_hal_status,"SIL-FDCAN-004","DEF-FDCAN-HAL-STATUS");
}

void fdcan_model_conformance() {
    // Independent model gate for the RM0440 FDCAN Tx FIFO/queue rules cited in
    // model_citations.md. These checks do not call CANDriver production code.
    silcan::reset(); silcan::set_auto_ack(false);
    FDCAN_TxHeaderTypeDef header{}; uint8_t payload[8]{};
    silcan::script(HAL_ERROR);
    CHECK(HAL_FDCAN_AddMessageToTxFifoQ(&silcan::handle,&header,payload)==HAL_ERROR);
    CHECK(silcan::regs.TXBRP==0u && silcan::regs.TXBTO==0u && silcan::latest_buffer()==-1);
    for(unsigned expected=0;expected<3;++expected) {
        header.Identifier=0x400u+expected;
        CHECK(HAL_FDCAN_AddMessageToTxFifoQ(&silcan::handle,&header,payload)==HAL_OK);
        CHECK(silcan::latest_buffer()==static_cast<int>(expected));
        CHECK(silcan::regs.TXBRP==((1u<<(expected+1u))-1u));
        CHECK((silcan::regs.TXBTO&(1u<<expected))==0u);
    }
    CHECK(HAL_FDCAN_AddMessageToTxFifoQ(&silcan::handle,&header,payload)==HAL_BUSY);
    silcan::force_terminal_buffer_for_test(1,true);
    CHECK((silcan::regs.TXBRP&(1u<<1))==0u);
    CHECK((silcan::regs.TXBTO&(1u<<1))!=0u);
    CHECK(HAL_FDCAN_AddMessageToTxFifoQ(&silcan::handle,&header,payload)==HAL_OK);
    CHECK(silcan::latest_buffer()==1); // scan wraps and selects the sole free slot
    CHECK((silcan::regs.TXBTO&(1u<<1))==0u); // a new request clears old completion
    silcan::force_terminal_buffer_for_test(0,false);
    CHECK((silcan::regs.TXBRP&1u)==0u && (silcan::regs.TXBTO&1u)==0u);
    CHECK(silcan::protocol.LastErrorCode==3u && silcan::counters.TxErrorCnt==0u);
}

void virtual_peer_protocol() {
    silcan::reset(); CANDriver d{&silcan::handle,0,0x7ff}; d.configureTransmission();
    silcan::set_peer_mode(sil::vcan::NodeMode::Offline); silcan::set_retry_policy(2,100);
    CHECK(d.addMessageToQueue(message(0x411))); CHECK(d.transmitMessage());
    CHECK(silcan::bus.queued_frames().size()==1 && silcan::bus.attempted_frames().size()==1);
    CHECK(silcan::bus.received_frames().empty()); CHECK(silcan::regs.TXBRP==1u && silcan::regs.TXBTO==0u);
    CHECK(silcan::protocol.LastErrorCode==3u);
    silcan::set_peer_mode(sil::vcan::NodeMode::Active);
    silcan::advance_us(99); CHECK(silcan::bus.attempted_frames().size()==1 && silcan::regs.TXBRP==1u);
    silcan::advance_us(1); CHECK(silcan::bus.attempted_frames().size()==2);
    CHECK(silcan::bus.received_frames().size()==1 && silcan::regs.TXBRP==0u && silcan::regs.TXBTO==1u);

    silcan::reset(); CANDriver listen_only{&silcan::handle,0,0x7ff};listen_only.configureTransmission();
    silcan::set_peer_mode(sil::vcan::NodeMode::ListenOnly);silcan::set_retry_policy(1,100);
    CHECK(listen_only.addMessageToQueue(message(0x413)));CHECK(listen_only.transmitMessage());
    CHECK(silcan::bus.received_frames().size()==1 && silcan::protocol.LastErrorCode==3u);
    CHECK(silcan::regs.TXBRP==1u && silcan::regs.TXBTO==0u);

    silcan::reset(); CANDriver delayed{&silcan::handle,0,0x7ff};delayed.configureTransmission();
    silcan::set_frame_completion_latency_us(50);
    CHECK(delayed.addMessageToQueue(message(0x412)));CHECK(delayed.transmitMessage());
    CHECK(silcan::regs.TXBRP==1u && silcan::bus.received_frames().empty());
    silcan::advance_us(49);CHECK(silcan::regs.TXBRP==1u);silcan::advance_us(1);
    CHECK(silcan::regs.TXBRP==0u && silcan::regs.TXBTO==1u && silcan::bus.received_frames().size()==1);

    const std::array<std::pair<sil::vcan::ProtocolError,uint32_t>,6> errors{{
        {sil::vcan::ProtocolError::Stuff,1},{sil::vcan::ProtocolError::Form,2},
        {sil::vcan::ProtocolError::Ack,3},{sil::vcan::ProtocolError::Bit1,4},
        {sil::vcan::ProtocolError::Bit0,5},{sil::vcan::ProtocolError::Crc,6}}};
    for(const auto& [error,expected_lec]:errors){
        silcan::reset();CANDriver faulted{&silcan::handle,0,0x7ff};faulted.configureTransmission();
        silcan::set_retry_policy(1,100);silcan::script_protocol_error(error);
        CHECK(faulted.addMessageToQueue(message(0x420+expected_lec)));CHECK(faulted.transmitMessage());
        CHECK(silcan::protocol.LastErrorCode==expected_lec && silcan::regs.TXBRP==1u && silcan::regs.TXBTO==0u);
        CHECK(silcan::bus.received_frames().size()==(error==sil::vcan::ProtocolError::Ack?1u:0u));
        CHECK(silcan::counters.TxErrorCnt==0u); // error confinement counters are outside this bounded model
    }
}

void deterministic_property_corpus() {
    constexpr uint32_t seed=0x4D464532u;
    uint32_t state=seed;
    const auto next=[&state]() {
        state^=state<<13; state^=state>>17; state^=state<<5; return state;
    };
    for(unsigned iteration=0;iteration<2048;++iteration) {
        const uint32_t id=next()&0x7FFu;
        const uint8_t requested=static_cast<uint8_t>(next()%13u);
        std::array<uint8_t,8> payload{};
        for(auto& byte:payload) byte=static_cast<uint8_t>(next());
        CANMessage candidate{id,payload.data(),requested};
        CHECK(candidate.getID()==id);
        CHECK(candidate.getLength()==std::min<uint8_t>(requested,8));
        for(uint8_t i=0;i<candidate.getLength();++i) CHECK(candidate.getData()[i]==payload[i]);
        std::array<uint8_t,8> replacement{};
        for(auto& byte:replacement) byte=static_cast<uint8_t>(next());
        const uint8_t replacement_length=static_cast<uint8_t>(next()%13u);
        candidate.setMessage(replacement.data(),replacement_length);
        CHECK(candidate.getLength()==std::min<uint8_t>(replacement_length,8));
        for(uint8_t i=0;i<candidate.getLength();++i) CHECK(candidate.getData()[i]==replacement[i]);
    }
    std::cout<<"PASS PROPERTY-CORPUS seed=0x4D464532 cases=2048\n";
}
}
int main(){fdcan_model_conformance();virtual_peer_protocol();std::cout<<"PASS MODEL-FDCAN RM0440_tx_queue_register_dynamics virtual_peer_ack_error_timing\n";std::cout<<"BLOCKED MODEL-FDCAN-EVIDENCE RM0440_revision_is_cited_but_not_archived_and_hash_locked\n";sil_fdcan_001();sil_fdcan_002_003();sil_fdcan_004();deterministic_property_corpus();std::cout<<"KNOWN_DEFECTS "<<known_defects<<'\n';return failures?EXIT_FAILURE:EXIT_SUCCESS;}
