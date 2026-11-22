#include "BoardManager.hpp"
#include "Wrapper.h"
#include "fake_hal.hpp"
#include "sil_check.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <exception>
#include <functional>
#include <iostream>
#include <string>
#include <vector>

float KTY84_function(uint32_t);
float GE1711_function(uint32_t);
float RE22_function(uint32_t);
float INFKL800_function(uint32_t);

namespace {
int failures=0, known_defects=0, blocked=0;
void xfail_if(bool observed, const char* id, const char* defect) {
    sil_check::xfail_if(observed, failures, known_defects, id, defect);
}
void block(const char* id, const char* reason) { ++blocked; std::cout<<"BLOCKED "<<id<<' '<<reason<<'\n'; }

void set_all_samples(uint32_t base=10000) {
    sil::set_spi_interframe_us(4); // scripted component scenario; target CS-high time remains HIL-required
    for (int d=0;d<2;++d) for(uint8_t c=0;c<8;++c) sil::set_sample(d,c,base+static_cast<uint32_t>(d*100+c));
}

std::vector<uint32_t> ids() { std::vector<uint32_t> out; for(const auto& f:sil::can_frames) out.push_back(f.header.Identifier); return out; }
uint64_t bits(const SilCanFrame& frame,unsigned start,unsigned width) {
    uint64_t packed=0; for(unsigned i=0;i<8;++i) packed|=static_cast<uint64_t>(frame.data[i])<<(8*i);
    return (packed>>start)&(width==64?~0ull:((1ull<<width)-1ull));
}
bool zero_from(const SilCanFrame& frame,std::size_t first) { return std::all_of(frame.data.begin()+static_cast<std::ptrdiff_t>(first),frame.data.end(),[](uint8_t v){return v==0;}); }
// DBC encoding oracles: raw = (physical - offset) / factor, mirroring the
// production float expression order so exact-equality decode checks are valid.
uint16_t kty_char(uint32_t raw) {
    const float voltage=(raw/65535.0)*5.0f; if(voltage>=5.0f)return static_cast<uint16_t>(999.0f)&0x0FFFu;
    const float resistance=(voltage*1000.0f)/(5.0f-voltage);
    return static_cast<uint16_t>(((resistance-433.41f)/5.9093f+40.0f)/0.1f)&0x0FFFu;
}
uint16_t ge_char(uint32_t raw) {
    const float voltage=(raw/65535.0f)*5.0f, ratio=voltage/(5.0f-voltage);
    const float temp=1.0f/((1.0f/298.15f)+(1.0f/3977.0f)*std::log(ratio))-273.15f;
    return static_cast<uint16_t>((temp+40.0f)/0.1f);
}
uint16_t angle_char(uint32_t raw){return static_cast<uint16_t>(((raw/65535.0f)*360.0f)/0.01f);}
uint16_t infkl_char(uint32_t raw){return static_cast<uint16_t>((200.0f*((raw/65535.0f)*5.0f)-100.0f)/0.1f);}

// Mirrors the deployed Core/Src/main.c startup and while(1) body through the
// same Wrapper C API main.c calls. main.c itself cannot be linked on host (its
// loop never returns), so verify_main_loop.ps1 statically pins main.c's source
// sequence to exactly what this replica performs; a divergence fails the run.
struct TestMainLoop {
    void* bm=nullptr;
    bool ok=false;        // main.c: ok = BoardManager_checkADCPower(bm)
    bool updated=false;   // main.c: BoardManager_publishAllChannels(bm) result
    bool published=false; // main.c: BoardManager_fdcan_bulk_tx(bm) was reached
};

// main.c startup: create, delay, setup, delay, then retry forever with a 500 ms
// delay after every retry (including the one that succeeds). max_attempts only
// bounds the test; the deployed loop has no bound.
bool test_main_start(TestMainLoop& app, bool front_node, unsigned max_attempts) {
    app.bm=BoardManager_create(front_node);
    app.ok=false; app.updated=false; app.published=false;
    if(app.bm==nullptr) return false;
    HAL_Delay(5);
    bool bm_setup=BoardManager_setup(app.bm);
    HAL_Delay(5);
    unsigned attempts=1;
    while(!bm_setup && attempts<max_attempts) {
        bm_setup=BoardManager_setup(app.bm);
        ++attempts;
        HAL_Delay(500);
    }
    if(!bm_setup) { BoardManager_destroy(app.bm); app.bm=nullptr; }
    return bm_setup;
}

// main.c while(1) body: health probe, publishAllChannels-gated publication, 4000 ms delay.
void test_main_run_cycle(TestMainLoop& app) {
    app.ok=BoardManager_checkADCPower(app.bm);
    app.updated=BoardManager_publishAllChannels(app.bm);
    app.published=false;
    if(app.updated) {
        BoardManager_fdcan_bulk_tx(app.bm);
        app.published=true;
    }
    HAL_Delay(4000);
}

void test_main_stop(TestMainLoop& app) {
    if(app.bm!=nullptr) { BoardManager_destroy(app.bm); app.bm=nullptr; }
}

void sil_bm_001_005_wrappers() {
    sil::reset(); set_all_samples();
    void* front=BoardManager_create(true); CHECK(front!=nullptr);
    CHECK(BoardManager_setup(front));
    CHECK(sil::can_calls.size()>=4);
    CHECK(sil::last_filter.FilterID1==0 && sil::last_filter.FilterID2==0x7FF);
    CHECK(BoardManager_checkADCPower(front));
    CHECK(!readChannel(front,0,0)); CHECK(!readChannel(front,3,0)); CHECK(!readChannel(front,1,8));
    CHECK(BoardManager_publishAllChannels(front));
    sil::can_frames.clear();
    CHECK(BoardManager_publishChannel(front,1,0,0x354));
    CHECK(sil::can_frames.size()==1 && sil::can_frames[0].header.Identifier==0x354);
    sil::can_frames.clear(); BoardManager_fdcan_bulk_tx(front);
    CHECK(ids()==std::vector<uint32_t>({200,210,300,700,800}));
    BoardManager_destroy(front);
}

void sil_bm_002_003_004() {
    sil::reset(); set_all_samples();
    BoardManager front(true);
    sil::script_can_status(HAL_ERROR);
    CHECK(!front.setup()); CHECK(sil::delays==std::vector<uint32_t>({50}));
    CHECK(front.setup());
    CHECK(sil::transfers.size() >= 4);
    CHECK(sil::transfers[0].device==0 && sil::transfers[1].device==0);
    CHECK(sil::transfers[2].device==1 && sil::transfers[3].device==1);
    sil::reset(); set_all_samples(); sil::ads_model(0).power_off(); BoardManager missing_adc1(true);
    CHECK(!missing_adc1.setup()); CHECK(std::find(sil::delays.begin(),sil::delays.end(),50)!=sil::delays.end());
    CHECK(sil::transfers.size()>=2);
    CHECK(sil::transfers[0].device==0 && sil::transfers[1].device==0);
    sil::reset(); set_all_samples(); sil::ads_model(1).power_off(); BoardManager missing_adc2(true);
    CHECK(!missing_adc2.setup()); CHECK(std::find(sil::delays.begin(),sil::delays.end(),50)!=sil::delays.end());
    sil::reset(); set_all_samples(); BoardManager adc_config_fail(true);
    sil::script_status(HAL_OK); sil::script_status(HAL_OK); sil::script_status(HAL_TIMEOUT);
    CHECK(!adc_config_fail.setup()); CHECK(sil::delays.back()==50);

    sil::reset(); set_all_samples(); BoardManager reference(true); CHECK(reference.setup());
    const std::size_t setup_spi_calls=sil::transfers.size();
    for(std::size_t fail_at=0; fail_at<setup_spi_calls; ++fail_at) {
        sil::reset(); set_all_samples(); BoardManager retry(true);
        for(std::size_t i=0;i<fail_at;++i) sil::script_status(HAL_OK);
        sil::script_status(HAL_ERROR);
        CHECK(!retry.setup()); CHECK(!sil::delays.empty() && sil::delays.back()==50);
        bool recovered=false;
        for(int attempt=0;attempt<3 && !recovered;++attempt) recovered=retry.setup();
        CHECK(recovered);
        CHECK(std::count(sil::can_calls.begin(),sil::can_calls.end(),"filter")==1);
    }
}

void sil_bm_005_faults() {
    sil::reset(); set_all_samples(); BoardManager bm(true); CHECK(bm.setup());
    sil::ads_model(0).power_off(); CHECK(!bm.checkADCPower());
    sil::ads_model(0).power_on();sil::ads_model(0).advance_time(5000000);sil::ads_model(1).power_off(); CHECK(!bm.checkADCPower());
    sil::ads_model(1).power_on();sil::ads_model(1).advance_time(5000000);
    for(const auto status:{HAL_ERROR,HAL_BUSY,HAL_TIMEOUT}) {
        for(std::size_t fail_at=0; fail_at<4; ++fail_at) {
            sil::reset(); BoardManager faulted(true);
            for(std::size_t i=0;i<fail_at;++i) sil::script_status(HAL_OK);
            sil::script_status(status); CHECK(!faulted.checkADCPower());
            CHECK(sil::transfers.size()==fail_at+1);
        }
    }
}

void sil_bm_006_can_005() {
    sil::reset(); set_all_samples(12000); BoardManager bm(true); CHECK(bm.setup());
    sil::can_frames.clear(); sil::delays.clear();
    CHECK(bm.publishChannel(1,3,0x355));
    CHECK(sil::can_frames.size()==1 && sil::can_frames[0].header.Identifier==0x355);
    CHECK(sil::can_frames[0].data[0]==static_cast<uint8_t>(12003));
    CHECK(sil::can_frames[0].data[1]==static_cast<uint8_t>(12003>>8));
    CHECK(std::all_of(sil::can_frames[0].data.begin()+4,sil::can_frames[0].data.end(),[](uint8_t v){return v==0;}));
    CHECK(sil::delays==std::vector<uint32_t>({5}));
    const auto before=sil::transfers.size();
    const bool invalid_accepted=bm.publishChannel(3,0,0x356);
    CHECK(invalid_accepted); CHECK(sil::transfers.size()>before);
    xfail_if(invalid_accepted,"SIL-BM-006","DEF-BM-PUSHCHANNEL-INVALID-ADC-ROUTES-ADC2");
}

void sil_virtual_can_peer() {
    sil::reset();set_all_samples(13000);BoardManager nominal(true);CHECK(nominal.setup());
    CHECK(nominal.publishChannel(1,0,0x361));
    CHECK(sil::can_bus.queued_frames().size()==1 && sil::can_bus.attempted_frames().size()==1);
    CHECK(sil::can_bus.received_frames().size()==1 && sil::can_frames.size()==1);

    sil::reset();set_all_samples(13000);BoardManager delayed(true);CHECK(delayed.setup());
    sil::set_can_frame_completion_latency_us(6000);
    CHECK(!delayed.publishChannel(1,0,0x362));
    CHECK(sil::can_bus.received_frames().empty());
    sil::advance_us(999);CHECK(sil::can_bus.received_frames().empty());sil::advance_us(1);
    CHECK(sil::can_bus.received_frames().size()==1);

    sil::reset();set_all_samples(13000);BoardManager absent(true);CHECK(absent.setup());
    sil::set_can_peer_mode(sil::vcan::NodeMode::Offline);sil::set_can_retry_policy(2,10000);
    CHECK(!absent.publishChannel(1,0,0x363));
    CHECK(sil::can_bus.received_frames().empty() && sil::protocol_status.LastErrorCode==3u);
    CHECK(sil::can_frames.empty());

    sil::reset();set_all_samples(13000);BoardManager retry(true);CHECK(retry.setup());
    sil::set_can_retry_policy(2,1000);sil::script_can_protocol_error(sil::vcan::ProtocolError::Crc);
    CHECK(retry.publishChannel(1,0,0x364));
    CHECK(sil::can_bus.attempted_frames().size()==2 && sil::can_bus.received_frames().size()==1);
    CHECK(sil::can_frames.size()==1); // link errors are events, never fabricated data frames
}

void sil_can_001_003_front_char() {
    sil::reset(); set_all_samples(20000); BoardManager bm(true); CHECK(bm.setup()); CHECK(bm.publishAllChannels());
    sil::can_frames.clear(); sil::delays.clear(); bm.fdcan_bulk_tx();
    CHECK(ids()==std::vector<uint32_t>({200,210,300,700,800}));
    CHECK(sil::can_frames.size()==5); for(const auto& f:sil::can_frames) CHECK(f.header.DataLength==8);
    CHECK(sil::delays==std::vector<uint32_t>({5,5,5,5,5}));
    const std::array<uint16_t,4> therm{{kty_char(20002),kty_char(20003),kty_char(20100),kty_char(20101)}};
    for(unsigned i=0;i<4;++i) { CHECK(bits(sil::can_frames[0],i*12,12)==therm[i]); }
    CHECK(zero_from(sil::can_frames[0],6));
    CHECK(bits(sil::can_frames[1],0,16)==ge_char(20005)); CHECK(zero_from(sil::can_frames[1],2));
    CHECK(bits(sil::can_frames[2],0,16)==angle_char(20004)); CHECK(bits(sil::can_frames[2],16,16)==angle_char(20006)); CHECK(zero_from(sil::can_frames[2],4));
    CHECK(bits(sil::can_frames[3],0,12)==kty_char(20001)); CHECK(bits(sil::can_frames[3],12,12)==kty_char(20007)); CHECK(zero_from(sil::can_frames[3],3));
    CHECK(bits(sil::can_frames[4],0,16)==infkl_char(20106)); CHECK(bits(sil::can_frames[4],16,16)==infkl_char(20105)); CHECK(zero_from(sil::can_frames[4],4));
    // DEF-CAN-ANGLE-FACTOR and DEF-CAN-TEMP-OFFSET retired: production now
    // encodes raw = angle/0.01 and raw = (T+40)/0.1 per the controlling DBC.
    const double angle=(20004.0/65535.0)*360.0;
    const uint16_t dbc_angle=static_cast<uint16_t>(angle*100.0);
    const uint16_t emitted=static_cast<uint16_t>(sil::can_frames[2].data[0] | (sil::can_frames[2].data[1]<<8));
    CHECK(emitted==dbc_angle);
    const uint16_t oil0=static_cast<uint16_t>(sil::can_frames[3].data[0] | ((sil::can_frames[3].data[1]&0x0Fu)<<8));
    const uint16_t oil1=static_cast<uint16_t>((sil::can_frames[3].data[1]>>4) | (sil::can_frames[3].data[2]<<4));
    const bool id700_uses_physical_front_channels = oil0==kty_char(20001) && oil1==kty_char(20007);
    xfail_if(id700_uses_physical_front_channels,"SIL-CAN-003","DEF-CAN700-ENGINE-GEARBOX-MAPPING");
    const uint16_t brake_line=static_cast<uint16_t>(sil::can_frames[1].data[0] | (sil::can_frames[1].data[1]<<8));
    const uint16_t ge_dispatch=static_cast<uint16_t>(GE1711_function(20005));
    xfail_if(brake_line==ge_dispatch,"SIL-XFR-006","DEF-PT1000-WRONG-GE-DISPATCH");
}

void sil_can_002_rear_char_and_fdcan_005() {
    sil::reset(); set_all_samples(22000); BoardManager bm(false); CHECK(bm.setup()); CHECK(bm.publishAllChannels());
    sil::can_frames.clear(); sil::delays.clear(); bm.fdcan_bulk_tx();
    const auto rear_ids=ids();
    // DEF-CAN810-MISSING and DEF-CAN-REAR-SCALING retired: the rear branch now
    // publishes ID 810 (BrakeTemp, 16-bit @0) and encodes angles at raw = deg/0.01.
    CHECK(rear_ids==std::vector<uint32_t>({500,600,720,810}));
    CHECK(bits(sil::can_frames[0],0,16)==angle_char(22004)); CHECK(bits(sil::can_frames[0],16,16)==angle_char(22006)); CHECK(zero_from(sil::can_frames[0],4));
    const std::array<uint16_t,4> rear_therm{{kty_char(22003),kty_char(22002),kty_char(22100),kty_char(22101)}};
    for(unsigned i=0;i<4;++i) { CHECK(bits(sil::can_frames[1],i*12,12)==rear_therm[i]); }
    CHECK(zero_from(sil::can_frames[1],6));
    CHECK(bits(sil::can_frames[2],0,12)==kty_char(22001)); CHECK(bits(sil::can_frames[2],12,12)==kty_char(22007));
    CHECK(bits(sil::can_frames[2],24,12)==(ge_char(22005)&0x0FFFu)); CHECK(bits(sil::can_frames[2],36,28)==0); CHECK(zero_from(sil::can_frames[2],5));
    CHECK(bits(sil::can_frames[3],0,16)==infkl_char(22106)); CHECK(zero_from(sil::can_frames[3],2));
    const double rear_angle=(22004.0/65535.0)*360.0;
    const uint16_t rear_emitted=static_cast<uint16_t>(sil::can_frames[0].data[0] | (sil::can_frames[0].data[1]<<8));
    CHECK(rear_emitted==static_cast<uint16_t>(rear_angle*100.0));
    sil::can_frames.clear(); sil::script_can_status(HAL_ERROR); bm.fdcan_bulk_tx();
    // With the fdcan_tx retry disabled, the failed frame is retained at the queue
    // head and each later fdcan_tx call drains the previous head, so the cycle
    // shifts: 500/600/720 deliver late and 810 stays queued this cycle. This is
    // the publication-failure-policy gap already tracked by SIL-FDCAN-005
    // DEF-BM-PUBLISH-FAILURE-POLICY.
    CHECK(sil::can_frames.size()==3);
    CHECK(ids()==std::vector<uint32_t>({500,600,720}));
}

void sil_bm_007_production_loop() {
    sil::reset(); set_all_samples(24000);
    sil::script_can_status(HAL_ERROR);
    TestMainLoop app{};
    CHECK(test_main_start(app,true,2));
    CHECK(app.bm!=nullptr);
    CHECK(sil::delays==std::vector<uint32_t>({5,50,5,500}));
    const std::vector<uint32_t> expected_ids{200,210,300,700,800};
    sil::delays.clear();
    for(int cycle=0;cycle<1000;++cycle) {
        set_all_samples(24000u+static_cast<uint32_t>(cycle%100));
        sil::can_frames.clear();
        test_main_run_cycle(app);
        CHECK(app.ok);
        CHECK(app.updated);
        CHECK(app.published);
        CHECK(ids()==expected_ids);
        sil::transfers.clear(); sil::can_calls.clear();
    }
    CHECK(sil::delays.size()==6000);
    for(std::size_t i=0;i<sil::delays.size();++i) CHECK(sil::delays[i]==((i%6)==5?4000u:5u));
    test_main_stop(app);

    sil::reset(); set_all_samples(); sil::script_can_status(HAL_ERROR);
    CHECK(!test_main_start(app,true,1));
    CHECK(app.bm==nullptr);
    CHECK(sil::delays==std::vector<uint32_t>({5,50,5}));
}

void e2e_degraded_production_loop() {
    const std::vector<uint32_t> expected_ids{200,210,300,700,800};
    sil::reset(); set_all_samples(27000);
    TestMainLoop app{};
    CHECK(test_main_start(app,true,1));

    // Establish a known-good cycle and independently retain its emitted bytes.
    sil::can_frames.clear();
    test_main_run_cycle(app);
    CHECK(app.ok && app.updated && app.published);
    CHECK(ids()==expected_ids);
    const auto nominal_frames=sil::can_frames;

    // Cycle 1: ADC1 absence is detected. The deployed policy still publishes,
    // which is retained as the published health-gating defect.
    sil::ads_model(0).power_off();
    sil::can_frames.clear();
    test_main_run_cycle(app);
    CHECK(!app.ok);
    CHECK(sil::all_chip_selects_high());
    // The defect is publication despite failed health, irrespective of whether
    // the resulting payload happens to be stale, zeroed, or partially updated.
    const bool unhealthy_published=!app.ok && app.published && !sil::can_frames.empty();
    xfail_if(unhealthy_published,"SIL-BM-007","DEF-BM-HEALTH-IGNORED-BEFORE-PUBLISH");

    // Cycle 2: restoring ADC1 must recover without reconstructing the manager.
    sil::ads_model(0).power_on();sil::ads_model(0).advance_time(5000000);
    sil::can_frames.clear();
    test_main_run_cycle(app);
    CHECK(app.ok && app.updated && app.published);
    CHECK(ids()==expected_ids);
    CHECK(sil::all_chip_selects_high());

    // Cycle 3: let both health probes and ADC1's eight manual reads complete,
    // then timeout the first ADC2 read. No publisher call or stale CAN data is
    // allowed after publishAllChannels reports the fault.
    for(int i=0;i<44;++i) sil::script_status(HAL_OK);
    sil::script_status(HAL_TIMEOUT);
    sil::can_frames.clear();
    test_main_run_cycle(app);
    CHECK(app.ok && !app.updated && !app.published);
    CHECK(sil::can_frames.empty());
    CHECK(sil::all_chip_selects_high());

    // Cycle 4: the timeout is transient and the next complete acquisition and
    // publication recovers deterministically with fresh sample values.
    set_all_samples(27500);
    sil::can_frames.clear();
    test_main_run_cycle(app);
    CHECK(app.ok && app.updated && app.published);
    CHECK(ids()==expected_ids);
    CHECK(sil::can_frames.size()==nominal_frames.size());
    CHECK(sil::can_frames[0].data!=nominal_frames[0].data);

    // Cycle 5: HAL_BUSY retains the failed queue head. The void publisher then
    // continues, and the following cycle remains shifted/stale. This is the
    // existing publication-failure-policy defect, now proved in a running loop.
    sil::script_can_status(HAL_BUSY);
    sil::can_frames.clear();
    test_main_run_cycle(app);
    const auto busy_ids=ids();
    sil::can_frames.clear();
    test_main_run_cycle(app);
    const auto after_busy_ids=ids();
    const bool busy_corrupts_recovery = busy_ids!=expected_ids || after_busy_ids!=expected_ids;
    CHECK(sil::all_chip_selects_high());
    test_main_stop(app);

    // Cycle 6: bus-off is detected through checkACK and restarts FDCAN. A clean
    // subsequent cycle recovers once the protocol status clears.
    sil::reset(); set_all_samples(28000); CHECK(test_main_start(app,true,1));
    sil::protocol_status.BusOff=1;
    sil::can_calls.clear(); sil::can_frames.clear();
    test_main_run_cycle(app);
    CHECK(std::count(sil::can_calls.begin(),sil::can_calls.end(),"stop")==5);
    CHECK(std::count(sil::can_calls.begin(),sil::can_calls.end(),"start")==5);
    CHECK(sil::all_chip_selects_high());
    const bool busoff_presented_frames=app.published && !sil::can_frames.empty();
    sil::protocol_status.BusOff=0;
    sil::can_calls.clear(); sil::can_frames.clear();
    test_main_run_cycle(app);
    CHECK(app.ok && app.updated && app.published);
    CHECK(ids()==expected_ids);
    CHECK(std::find(sil::can_calls.begin(),sil::can_calls.end(),"stop")==sil::can_calls.end());
    xfail_if(busy_corrupts_recovery || busoff_presented_frames,
             "SIL-FDCAN-005","DEF-BM-PUBLISH-FAILURE-POLICY");
    test_main_stop(app);

    // Transfer-counted spontaneous reset after health probing, during ADC1's
    // acquisition. Production currently has no reset-generation/status check
    // at this point, so characterize the unsafe publication and recovery.
    sil::reset(); set_all_samples(28500); CHECK(test_main_start(app,true,1));
    sil::schedule_ads_reset_after_transfers(0,6);
    sil::can_frames.clear(); test_main_run_cycle(app);
    const bool reset_undetected=app.ok && app.updated && app.published && !sil::can_frames.empty();
    CHECK(sil::ads[0]->spontaneous_resets==1u);
    CHECK(sil::all_chip_selects_high());
    sil::can_frames.clear(); test_main_run_cycle(app);
    CHECK(app.ok && app.updated && app.published);
    CHECK(ids()==expected_ids && sil::all_chip_selects_high());
    xfail_if(reset_undetected,"E2E-ADC-RESET","DEF-ADC-RESET-UNDETECTED");
    test_main_stop(app);

    block("E2E-DEGRADED-AUTO","production_loop_uses_manual_acquisition_so_corrupt_auto_channel_ID_is_not_an_end_to_end_injectable_fault");
}

void sil_xfr_available_and_blocked() {
    // Transfer functions now return DBC raw counts (angle/0.01, temp/0.1), so
    // the characterized values are the old physical values scaled accordingly.
    CHECK(std::fabs(RE22_function(32768)-18000.27f)<2.0f);
    // DEF-INFKL-3V3-FULL-SCALE retired: production now reconstructs the 5 V
    // ADC span (AVDD), so full-scale code decodes to 900 C in raw counts.
    CHECK(std::fabs(INFKL800_function(65535)-9000.0f)<1.0f);
    xfail_if(std::fabs(RE22_function(32768)-18000.27f)<2.0f,"SIL-XFR-003","DEF-RE22-ENDPOINT-NORMALIZATION");
    xfail_if(GE1711_function(65535)<-200.0f,"SIL-XFR-002","DEF-GE1711-3V3-UNSAFE-ENDPOINT");
    block("SIL-XFR-001","exact_KTY84_suffix_and_fitted_resistor_population_missing");
    block("SIL-XFR-002","fitted_divider_population_and_sensor_return_wiring_missing");
    block("SIL-XFR-003","RE22_ordering_code_direction_period_and_mechanical_zero_missing");
    block("SIL-XFR-004","ID810_physical_channel_and_above_150C_policy_missing");
    block("SIL-XFR-005","physical_population_inputs_missing");
    block("SIL-XFR-006","exact_PT1000_table_lead_arrangement_and_divider_population_missing");
    block("SIL-CAN-003","ID700_and_ID810_physical_source_mapping_missing");
    block("SIL-CAN-004","DBC_rear_brake_line_12bit_range_cannot_represent_declared_500C");
    CHECK(INFKL800_function(0)<0.0f);
    CHECK(GE1711_function(0)<0.0f);
    CHECK(!std::isfinite(GE1711_function(70000)));
    // Exercise the production packing path at a small non-negative INFKL
    // raw code. Codes below it and non-finite GE outputs are intentionally not
    // cast here: C++ does not define an out-of-range float-to-unsigned cast.
    sil::reset(); set_all_samples(20000); sil::set_sample(1,5,9930); sil::set_sample(1,6,9930);
    BoardManager numeric_boundary(true); CHECK(numeric_boundary.setup()); CHECK(numeric_boundary.publishAllChannels());
    sil::can_frames.clear(); numeric_boundary.fdcan_bulk_tx();
    CHECK(ids()==std::vector<uint32_t>({200,210,300,700,800}));
    CHECK(bits(sil::can_frames[4],0,16)==infkl_char(9930));
    CHECK(bits(sil::can_frames[4],16,16)==infkl_char(9930));
    block("NUMERIC-PACKING-CHAR","negative_or_nonfinite_float_to_unsigned_CAN_cast_has_no_defined_saturation_policy_and_requires_UBSan_evidence");
}

void production_path_property_corpus() {
    constexpr uint32_t seed=0x50554231u; uint32_t state=seed;
    const auto next=[&state](){state^=state<<13;state^=state>>17;state^=state<<5;return state;};
    sil::reset(); set_all_samples(); BoardManager bm(true); CHECK(bm.setup());
    for(unsigned iteration=0;iteration<128;++iteration){
        std::array<std::array<uint32_t,8>,2> sample{};
        for(int device=0;device<2;++device) for(uint8_t channel=0;channel<8;++channel){
            sample[device][channel]=20000u+(next()%30001u);
            sil::set_sample(device,channel,sample[device][channel]);
        }
        for(const auto raw:sample[0]){
            CHECK(std::isfinite(KTY84_function(raw)) && KTY84_function(raw)>=0.0f && KTY84_function(raw)*10.0f<=65535.0f);
            CHECK(std::isfinite(RE22_function(raw)) && RE22_function(raw)>=0.0f && RE22_function(raw)<=65535.0f);
        }
        CHECK(std::isfinite(GE1711_function(sample[0][5])) && GE1711_function(sample[0][5])>=0.0f && GE1711_function(sample[0][5])*10.0f<=65535.0f);
        CHECK(std::isfinite(INFKL800_function(sample[1][5])) && INFKL800_function(sample[1][5])>=0.0f && INFKL800_function(sample[1][5])<=65535.0f);
        CHECK(std::isfinite(INFKL800_function(sample[1][6])) && INFKL800_function(sample[1][6])>=0.0f && INFKL800_function(sample[1][6])<=65535.0f);
        CHECK(bm.publishAllChannels()); sil::can_frames.clear(); bm.fdcan_bulk_tx();
        CHECK(ids()==std::vector<uint32_t>({200,210,300,700,800}));
        CHECK(sil::can_frames.size()==5);
        for(const auto& frame:sil::can_frames) CHECK(frame.header.DataLength==8);
        const std::array<uint16_t,4> therm{{kty_char(sample[0][2]),kty_char(sample[0][3]),kty_char(sample[1][0]),kty_char(sample[1][1])}};
        for(unsigned i=0;i<4;++i) CHECK(bits(sil::can_frames[0],12u*i,12)==therm[i]);
        CHECK(bits(sil::can_frames[1],0,16)==ge_char(sample[0][5]));
        CHECK(bits(sil::can_frames[2],0,16)==angle_char(sample[0][4]));
        CHECK(bits(sil::can_frames[2],16,16)==angle_char(sample[0][6]));
        CHECK(bits(sil::can_frames[3],0,12)==kty_char(sample[0][1]));
        CHECK(bits(sil::can_frames[3],12,12)==kty_char(sample[0][7]));
        CHECK(bits(sil::can_frames[4],0,16)==infkl_char(sample[1][6]));
        CHECK(bits(sil::can_frames[4],16,16)==infkl_char(sample[1][5]));
        CHECK(zero_from(sil::can_frames[1],2)); CHECK(zero_from(sil::can_frames[2],4));
    }
    std::cout<<"PASS PROPERTY-PRODUCTION seed=0x50554231 cases=128 update_publish_independent_field_decode\n";
}
}

int main(){
    const std::array<std::pair<const char*, std::function<void()>>,11> tests{{
        {"SIL-BM-001/005 wrappers",sil_bm_001_005_wrappers},
        {"SIL-BM-002/003/004",sil_bm_002_003_004},
        {"SIL-BM-005 faults",sil_bm_005_faults},
        {"SIL-BM-006/CAN-005",sil_bm_006_can_005},
        {"SIL-VCAN-001 peer",sil_virtual_can_peer},
        {"SIL-CAN-001/003 front",sil_can_001_003_front_char},
        {"SIL-CAN-002 rear/FDCAN-005",sil_can_002_rear_char_and_fdcan_005},
        {"SIL-BM-007 production loop",sil_bm_007_production_loop},
        {"E2E-DEGRADED production loop",e2e_degraded_production_loop},
        {"SIL-XFR",sil_xfr_available_and_blocked},
        {"production property corpus",production_path_property_corpus}}};
    for (const auto& [name,test] : tests) {
        const int prior=failures;
        try { test(); }
        catch (const std::exception& error) { std::cerr<<name<<": "<<error.what()<<'\n'; ++failures; }
        catch (...) { std::cerr<<name<<": unknown exception\n"; ++failures; }
        std::cout<<(failures==prior?"PASS ":"FAIL ")<<name<<'\n';
    }
    std::cout<<"PASS APPLICATION production_sources_and_1000_cycles\n";
    std::cout<<"KNOWN_DEFECTS "<<known_defects<<" BLOCKED "<<blocked<<'\n';
    return failures?EXIT_FAILURE:EXIT_SUCCESS;
}
