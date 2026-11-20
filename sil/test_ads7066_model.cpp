#include "ads7066_bus.hpp"
#include "ads7066_fault_overlay.hpp"
#include "ads7066_model.hpp"

#include <array>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

using namespace sil::ads7066;
namespace {
int failures{};
#define CHECK(x) do { if (!(x)) throw std::runtime_error(std::string("CHECK failed: ") + #x); } while (false)

void ready(Ads7066Model& device) { device.advance_time(5000000); }

std::vector<uint8_t> frame(Ads7066Model& device,const std::vector<uint8_t>& tx) {
    device.set_cs(false); std::vector<uint8_t> rx(tx.size(),0);
    for (std::size_t byte = 0; byte < tx.size(); ++byte) for (int bit = 7; bit >= 0; --bit) if (device.clock_spi_bit((tx[byte] & (1u << bit)) != 0u) == PinLevel::High) rx[byte] |= static_cast<uint8_t>(1u << bit);
    device.set_cs(true); return rx;
}

void write_reg(Ads7066Model& device,uint8_t address,uint8_t value,uint8_t opcode=0x08) { (void)frame(device,{opcode,address,value}); }
uint8_t read_reg(Ads7066Model& device,uint8_t address) { (void)frame(device,{0x10,address,0}); return frame(device,{0,0,0})[0]; }

std::vector<uint8_t> overlay_frame(Ads7066FaultOverlay& overlay,const std::vector<uint8_t>& tx) {
    overlay.set_cs(false);std::vector<uint8_t> rx(tx.size(),0);
    for(std::size_t byte=0;byte<tx.size();++byte)for(int bit=7;bit>=0;--bit)if(overlay.clock_spi_bit((tx[byte]&(1u<<bit))!=0u)==PinLevel::High)rx[byte]|=static_cast<uint8_t>(1u<<bit);
    overlay.set_cs(true);return rx;
}

uint8_t golden_crc8(const uint8_t* data,std::size_t size) {
    uint8_t crc=0xFF;
    for(std::size_t i=0;i<size;++i){crc^=data[i];for(unsigned bit=0;bit<8;++bit)crc=static_cast<uint8_t>((crc&0x80u)!=0u?static_cast<uint8_t>((crc<<1u)^0x07u):static_cast<uint8_t>(crc<<1u));}
    return crc;
}

void table_7_4_case(bool crc_enabled,bool averaging,uint8_t append,bool daisy_supported) {
    Ads7066Model d; ready(d); d.set_analog_code(5,0x1234); write_reg(d,0x11,5);
    write_reg(d,2,static_cast<uint8_t>(append<<4u));
    write_reg(d,3,averaging?1u:0u);
    if(crc_enabled) write_reg(d,1,0x40);
    const uint8_t nop[3]{0,0,0};
    if(crc_enabled)(void)frame(d,{0,0,0,golden_crc8(nop,3)});else (void)frame(d,{0,0,0});
    d.advance_time(averaging?8480u:3200u);
    const auto rx=crc_enabled?frame(d,{0,0,0,golden_crc8(nop,3)}):frame(d,{0,0,0});
    CHECK(rx[0]==0x12&&rx[1]==0x34);
    const uint8_t tail=append==1u?(averaging?0x05u:0x50u):(append==2u?(averaging?0x08u:0x80u):0u);
    if(crc_enabled){
        const std::size_t payload=(!averaging&&(append==0u||append==3u))?2u:3u;
        if(payload==2u){CHECK(rx[2]==golden_crc8(rx.data(),2)&&rx[3]==0);}else{CHECK(rx[2]==tail&&rx[3]==golden_crc8(rx.data(),3));}
    }else CHECK(rx.size()==3&&rx[2]==tail);
    CHECK(d.daisy_pass_through_enabled()==daisy_supported);
}

void table_7_3_case(uint8_t opmode,uint64_t cycle_ns) {
    Ads7066Model d; ready(d); write_reg(d,3,1); write_reg(d,4,opmode);
    (void)frame(d,{0,0,0});
    CHECK(d.conversion_pending());
    CHECK(d.conversion_ready_ns()-d.now_ns()==cycle_ns*2u*106u/100u);
}

void spi_mode_case(uint8_t mode) {
    Ads7066Model d;ready(d);write_reg(d,2,static_cast<uint8_t>(0x80u|mode));
    const std::array<uint8_t,3> command{{8,5,0xA5}};std::vector<bool> captured;captured.reserve(24);d.set_cs(false);
    const bool cpol=(mode&2u)!=0u,cpha=(mode&1u)!=0u;
    for(uint8_t byte:command)for(int bit=7;bit>=0;--bit){const bool sdi=(byte&(1u<<bit))!=0u;const PinLevel leading=d.clock_edge(!cpol,sdi);if(!cpha)captured.push_back(leading==PinLevel::High);else CHECK(d.sdo()==leading);const PinLevel trailing=d.clock_edge(cpol,sdi);if(cpha)captured.push_back(trailing==PinLevel::High);else CHECK(d.sdo()==trailing);}
    d.set_cs(true);CHECK(d.read_register(5)==0xA5);const std::array<uint8_t,3> expected{{0xA5,0xA5,0}};for(std::size_t i=0;i<24;++i)CHECK(captured[i]==((expected[i/8]&(1u<<(7u-i%8u)))!=0u));
}

void fault_power_case(){Ads7066Model d;ready(d);write_reg(d,5,0xA5);Ads7066FaultOverlay f(d);f.set_cs(false);for(unsigned i=0;i<5;++i)(void)f.clock_spi_bit(true);f.schedule({d.now_ns(),FaultKind::PowerReset,0,0});f.advance_time(0);CHECK(!d.responsive()&&d.sdo()==PinLevel::HighZ);f.advance_time(5000000);CHECK(d.responsive()&&d.sdo()!=PinLevel::HighZ&&d.read_register(5)==0);f.set_cs(true);CHECK(d.invalid_frames().empty());}
void fault_brownout_case(){Ads7066Model d;ready(d);write_reg(d,5,0xA5);Ads7066FaultOverlay f(d);const auto t=d.now_ns();f.schedule({t+25,FaultKind::Brownout,0,0});f.advance_time(25);CHECK(d.now_ns()==t+25&&!d.responsive()&&d.read_register(5)==0);f.advance_time(5000000);CHECK(d.responsive());}
void fault_gpio_case(){Ads7066Model d;ready(d);write_reg(d,5,1);Ads7066FaultOverlay f(d);f.schedule({d.now_ns()+100,FaultKind::GpioLevel,0,1});f.advance_time(99);CHECK((d.read_register(0x0D)&1u)==0);f.advance_time(1);CHECK((d.read_register(0x0D)&1u)!=0);}
void fault_code_boundary_case(){Ads7066Model d;ready(d);write_reg(d,3,1);d.set_analog_code(0,0x1000);Ads7066FaultOverlay f(d);(void)overlay_frame(f,{0,0,0});const auto sample=d.now_ns()+4000;f.schedule({sample,FaultKind::ConversionCode,0,0x5000});f.advance_time(8480);const auto rx=overlay_frame(f,{0,0,0});const uint32_t raw=(uint32_t{rx[0]}<<12u)|(uint32_t{rx[1]}<<4u)|(rx[2]>>4u);CHECK(raw==0x30000u);}
void fault_delay_case(){Ads7066Model d;ready(d);Ads7066FaultOverlay f(d);const uint64_t wide=uint64_t{1}<<32u;f.schedule({d.now_ns(),FaultKind::DelayReadiness,0,wide});(void)overlay_frame(f,{0,0,0});CHECK(d.conversion_ready_ns()-d.now_ns()==wide+3200u);f.set_cs(false);const auto before=d.conversion_ready_ns();f.schedule({d.now_ns(),FaultKind::DelayReadiness,0,77});f.advance_time(0);CHECK(d.conversion_ready_ns()==before+77u);f.set_cs(true);}
void fault_input_case(){Ads7066Model d;ready(d);Ads7066FaultOverlay f(d);f.schedule({d.now_ns(),FaultKind::CorruptInputBit,0,0});(void)overlay_frame(f,{8,5,0xA5});CHECK(d.read_register(5)==0&&!d.reserved_accesses().empty());}
void fault_crc_case(){Ads7066Model d;ready(d);write_reg(d,1,0x40);Ads7066FaultOverlay f(d);const uint8_t cmd[3]{8,5,0xA5};f.schedule({d.now_ns(),FaultKind::CorruptInputBit,0,0});(void)overlay_frame(f,{cmd[0],cmd[1],cmd[2],golden_crc8(cmd,3)});CHECK(d.read_register(5)==0&&(d.read_register(0)&2u)!=0);}
void fault_sdo_case(){Ads7066Model d;ready(d);write_reg(d,2,0x80);Ads7066FaultOverlay f(d);f.set_cs(false);f.schedule({d.now_ns(),FaultKind::FlipSdoBit,0,0});CHECK(f.clock_spi_bit(false)==PinLevel::Low);CHECK(f.clock_spi_bit(false)==PinLevel::Low);f.schedule({d.now_ns(),FaultKind::ForceSdoHigh,0,0});CHECK(f.clock_spi_bit(false)==PinLevel::High);f.schedule({d.now_ns(),FaultKind::ForceSdoLow,0,0});CHECK(f.clock_spi_bit(false)==PinLevel::Low);f.schedule({d.now_ns(),FaultKind::ReleaseSdo,0,0});CHECK(f.clock_spi_bit(false)==PinLevel::Low);f.set_cs(true);}
void fault_validation_case(){Ads7066Model d;ready(d);Ads7066FaultOverlay f(d);bool past=false,index=false,overflow=false;try{f.schedule({d.now_ns()-1,FaultKind::Brownout,0,0});}catch(const std::invalid_argument&){past=true;}try{f.schedule({d.now_ns(),FaultKind::ConversionCode,8,0});}catch(const std::out_of_range&){index=true;}f.schedule({d.now_ns(),FaultKind::DelayReadiness,0,std::numeric_limits<uint64_t>::max()});f.schedule({d.now_ns(),FaultKind::DelayReadiness,0,1});try{f.advance_time(0);}catch(const std::overflow_error&){overflow=true;}CHECK(past&&index&&overflow);}
void fault_precedence_case(){const std::array<FaultKind,10> ordered{{FaultKind::PowerReset,FaultKind::Brownout,FaultKind::DelayReadiness,FaultKind::ConversionCode,FaultKind::GpioLevel,FaultKind::CorruptInputBit,FaultKind::FlipSdoBit,FaultKind::ForceSdoLow,FaultKind::ForceSdoHigh,FaultKind::ReleaseSdo}};for(std::size_t i=0;i<ordered.size();++i){const unsigned expected=i<7?static_cast<unsigned>(i):7u;CHECK(Ads7066FaultOverlay::precedence(ordered[i])==expected);}Ads7066Model a;ready(a);Ads7066FaultOverlay fa(a);fa.set_cs(false);fa.schedule({a.now_ns(),FaultKind::ForceSdoHigh,0,0});fa.schedule({a.now_ns(),FaultKind::ReleaseSdo,0,0});fa.advance_time(0);CHECK(fa.clock_spi_bit(false)==PinLevel::Low);fa.set_cs(true);Ads7066Model b;ready(b);Ads7066FaultOverlay fb(b);fb.set_cs(false);fb.schedule({b.now_ns(),FaultKind::ReleaseSdo,0,0});fb.schedule({b.now_ns(),FaultKind::ForceSdoHigh,0,0});fb.advance_time(0);CHECK(fb.clock_spi_bit(false)==PinLevel::High);}

void daisy_crc_case(){Ads7066Model a,b,c;ready(a);ready(b);ready(c);write_reg(a,1,0x40);write_reg(b,1,0x40);write_reg(c,1,0x40);Ads7066DaisyChain chain({&a,&b,&c});chain.set_cs(false);const auto result=chain.transfer({8,5,0xBE,8,5,0xBE,8,5,0xBE,8});chain.set_cs(true);CHECK(result.result==BusResult::Ok&&a.read_register(5)==0xBE&&b.read_register(5)==0xBE&&c.read_register(5)==0xBE);}
void daisy_disabled_case(){Ads7066Model a,b;ready(a);ready(b);write_reg(a,2,0x20);write_reg(a,1,0x40);Ads7066DaisyChain chain({&a,&b});chain.set_cs(false);const auto before_a=a.now_ns(),before_b=b.now_ns();const auto result=chain.transfer({0,0,0,0,0,0,0,0});CHECK(result.result==BusResult::DaisyPassThroughDisabled&&a.now_ns()==before_a+1280&&b.now_ns()==before_b+1280);chain.set_cs(true);CHECK(a.conversion_pending()&&b.conversion_pending());}
void fault_isolation_case(){Ads7066Model a,b;ready(a);ready(b);Ads7066FaultOverlay fa(a),fb(b);fa.schedule({a.now_ns()+100,FaultKind::ConversionCode,0,0x1111});fb.schedule({b.now_ns()+100,FaultKind::ConversionCode,0,0x2222});fa.advance_time(100);CHECK(fb.pending_events()==1);fb.advance_time(100);write_reg(a,1,0x40);const uint8_t cmd[3]{8,5,0xA5};(void)overlay_frame(fa,{cmd[0],cmd[1],cmd[2],0});CHECK((a.read_register(0)&2u)!=0&&(b.read_register(0)&2u)==0);fb.schedule({b.now_ns(),FaultKind::DelayReadiness,0,99});(void)overlay_frame(fb,{0,0,0});CHECK(b.conversion_ready_ns()-b.now_ns()==3299);fa.schedule({a.now_ns(),FaultKind::Brownout,0,0});fa.advance_time(0);CHECK(!a.responsive()&&b.responsive());}

void reset_and_pin_conformance() {
    Ads7066Model d; CHECK(d.now_ns()==0); CHECK(!d.responsive()); CHECK(d.sdo()==PinLevel::HighZ);
    d.advance_time(4999999); CHECK(!d.responsive()); d.advance_time(1); CHECK(d.responsive());
    CHECK(d.read_register(0)==0x81 && d.read_register(4)==0x04);
    d.set_cs(false); CHECK(d.sdo()!=PinLevel::HighZ); d.set_cs(true); CHECK(d.sdo()==PinLevel::HighZ);
    const auto before=d.now_ns(); d.power_off(); d.advance_time(123); d.power_on(); CHECK(d.now_ns()==before+123); CHECK(!d.responsive());
    d.advance_time(5000000); CHECK(d.responsive() && d.read_register(0)==0x81);
    write_reg(d,1,1); CHECK(!d.responsive()); d.advance_time(4999999); CHECK(!d.responsive()); d.advance_time(1); CHECK(d.responsive());
    CHECK(d.read_register(0)==0x81 && d.read_register(1)==0 && d.read_register(4)==4);
}

void register_and_cross_register_conformance() {
    Ads7066Model d; ready(d);
    for (unsigned address=0; address<256; ++address) {
        if(address==0 || address==1 || address>=0xBF) continue;
        write_reg(d,static_cast<uint8_t>(address),0xFF);
        const auto& rules=Ads7066Model::register_rules(); uint8_t expected=0;
        for(const auto& rule:rules) if(rule.address==address && !rule.diagnostic_lock) expected=static_cast<uint8_t>((rule.reset&~rule.writable_mask)|rule.writable_mask);
        CHECK(read_reg(d,static_cast<uint8_t>(address))==expected);
    }
    write_reg(d,5,0xA5); write_reg(d,5,0x03,0x18); CHECK(read_reg(d,5)==0xA7); write_reg(d,5,0x81,0x20); CHECK(read_reg(d,5)==0x26);
    CHECK((read_reg(d,0)&1u)!=0u); write_reg(d,0,1); CHECK((read_reg(d,0)&1u)==0u);
    write_reg(d,5,1); write_reg(d,7,1); write_reg(d,9,1); write_reg(d,0x0B,1); CHECK(d.gpio_drive(0)==PinLevel::High);
    d.set_gpio_input(0,true); write_reg(d,7,0); CHECK((read_reg(d,0x0D)&1u)!=0u);
    write_reg(d,1,4); CHECK(d.gpio_drive(0)==PinLevel::HighZ && (read_reg(d,0x0D)&1u)==0u); CHECK(read_reg(d,5)==1);
    write_reg(d,0xC0,0x11); CHECK(read_reg(d,0xC0)==0); write_reg(d,0xBF,0x95); write_reg(d,0xC0,0x11); CHECK(read_reg(d,0xC0)==0);
    write_reg(d,0xBF,0x96); write_reg(d,0xC0,0x11); CHECK(read_reg(d,0xC0)==0x11); write_reg(d,0xBF,0); write_reg(d,0xC1,0x5A); CHECK(read_reg(d,0xC1)==0x5A);
}

void spi_frame_and_crc_conformance() {
    Ads7066Model d; ready(d); write_reg(d,5,0x11); d.set_cs(false);
    for(uint8_t byte:{uint8_t{0x08},uint8_t{5},uint8_t{0xAA}}) for(int bit=7;bit>=0;--bit)(void)d.clock_spi_bit((byte&(1u<<bit))!=0u);
    CHECK(d.read_register(5)==0x11); d.set_cs(true); CHECK(d.read_register(5)==0xAA);
    d.set_cs(false); for(int bit=0;bit<7;++bit)(void)d.clock_spi_bit(false); d.set_cs(true); CHECK(d.invalid_frames().back()==7);
    const std::array<std::array<uint8_t,4>,4> vectors{{{{0,0,0,0x2B}},{{0x08,1,0x48,0x90}},{{0x08,0,2,0x74}},{{0xFF,0xFF,0xFF,0x24}}}};
    for(const auto& v:vectors) CHECK(Ads7066Model::crc8(v.data(),3)==v[3]);
    write_reg(d,1,0x40); const uint8_t command[3]{0x08,5,0x33};
    (void)frame(d,{command[0],command[1],command[2],0}); CHECK((d.read_register(0)&2u)!=0u && d.read_register(5)==0xAA);
    const uint8_t clear[3]{0x08,0,2}; (void)frame(d,{clear[0],clear[1],clear[2],Ads7066Model::crc8(clear,3)}); CHECK((d.read_register(0)&2u)==0u);
    const uint8_t good[3]{0x08,5,0x33}; (void)frame(d,{good[0],good[1],good[2],Ads7066Model::crc8(good,3)}); CHECK(d.read_register(5)==0x33);
    const uint8_t rd[3]{0x10,5,0}; (void)frame(d,{rd[0],rd[1],rd[2],Ads7066Model::crc8(rd,3)});
    const uint8_t nop[3]{0,0,0}; const auto rx=frame(d,{0,0,0,Ads7066Model::crc8(nop,3)}); CHECK(rx[0]==0x33 && rx[1]==Ads7066Model::crc8(rx.data(),1) && rx[2]==0 && rx[3]==0);
}

void edge_mode_conformance() {
    for(uint8_t mode=0;mode<4;++mode) {
        Ads7066Model d; ready(d); write_reg(d,2,0x80u|mode); d.set_cs(false);
        const bool cpol=(mode&2u)!=0u,cpha=(mode&1u)!=0u; CHECK(d.sdo()==(cpha?PinLevel::Low:PinLevel::High));
        const bool leading=!cpol; const PinLevel first=d.clock_edge(leading,false);
        if(cpha) CHECK(first==PinLevel::High); else CHECK(first==PinLevel::High);
        const PinLevel between=d.sdo(); CHECK(between==PinLevel::High);
        const PinLevel trailing=d.clock_edge(cpol,false);
        if(cpha) {
            CHECK(trailing==PinLevel::High && d.sdo()==PinLevel::High);
            CHECK(d.clock_edge(leading,false)==PinLevel::Low && d.sdo()==PinLevel::Low);
        } else CHECK(trailing==PinLevel::Low && d.sdo()==PinLevel::Low); // A5 second bit
        d.set_cs(true); CHECK(d.sdo()==PinLevel::HighZ);
    }
}

void conversion_timing_and_output_conformance() {
    Ads7066Model d; ready(d); d.set_analog_code(0,0x1234);
    (void)frame(d,{0,0,0}); CHECK(d.conversion_pending()); const auto due=d.conversion_ready_ns(); CHECK(due-d.now_ns()<=3200u);
    const auto early=frame(d,{0,0,0}); CHECK(early[0]==0 && d.premature_reads()>0); d.advance_time(3200);
    const auto result=frame(d,{0,0,0}); CHECK(result[0]==0x12 && result[1]==0x34);
    write_reg(d,3,1); write_reg(d,4,4); d.queue_analog_code(0,0x1000,d.now_ns()); d.queue_analog_code(0,0x3000,d.now_ns()+4000);
    (void)frame(d,{0,0,0}); d.advance_time(8480); const auto avg=frame(d,{0,0,0}); const uint32_t raw=(static_cast<uint32_t>(avg[0])<<12u)|(static_cast<uint32_t>(avg[1])<<4u)|(avg[2]>>4u); CHECK(raw==0x20000u);
    write_reg(d,2,0x10); d.advance_time(8480); const auto tagged=frame(d,{0,0,0}); CHECK((tagged[2]&0x0Fu)==0);
    write_reg(d,2,0x80); d.advance_time(8480); const auto fixed=frame(d,{0,0,0}); CHECK(fixed[0]==0xA5 && fixed[1]==0xA5);
}

void mode_gpio_diagnostic_conformance() {
    Ads7066Model d; ready(d); d.set_analog_code(0,0x1000); d.set_analog_code(2,0x2000); d.set_analog_code(5,0x5000);
    write_reg(d,0x12,0x25); write_reg(d,0x10,0x11); CHECK((d.read_register(0)&0x40u)!=0u);
    (void)frame(d,{0,0,0}); d.advance_time(3200); const auto c0=frame(d,{0,0,0}); CHECK(c0[0]==0x10);
    write_reg(d,0x12,0x20); (void)frame(d,{0,0,0}); d.advance_time(3200); const auto c5=frame(d,{0,0,0}); CHECK(c5[0]==0x50);
    write_reg(d,0x10,0); CHECK((d.read_register(0)&0x40u)==0u);
    write_reg(d,0xBF,0x96); write_reg(d,0xC1,0x34); write_reg(d,0xC2,0x12); write_reg(d,0xC0,0x11); write_reg(d,3,1); d.set_vtest_code(0x7777);
    (void)frame(d,{0,0,0}); d.advance_time(8480); const auto bitwalk=frame(d,{0,0,0}); const uint32_t raw=(static_cast<uint32_t>(bitwalk[0])<<12u)|(static_cast<uint32_t>(bitwalk[1])<<4u)|(bitwalk[2]>>4u); CHECK(raw==0x12340u);
}

void bus_daisy_and_fault_conformance() {
    Ads7066Model a,b,c; ready(a);ready(b);ready(c); Ads7066Bus bus({&a,&b});
    bus.set_cs(0,false); auto ok=bus.transfer({0x08,5,0x11}); bus.set_cs(0,true); CHECK(ok.result==BusResult::Ok && a.read_register(5)==0x11 && b.read_register(5)==0);
    bus.set_cs(0,false);bus.set_cs(1,false); CHECK(bus.transfer({0,0,0}).result==BusResult::Contention); bus.set_cs(0,true);bus.set_cs(1,true);
    Ads7066DaisyChain chain({&a,&b,&c}); chain.set_cs(false); const auto daisy=chain.transfer({0x08,5,0xC3,0x08,5,0xB2,0x08,5,0xA1}); chain.set_cs(true);
    CHECK(daisy.result==BusResult::Ok && a.read_register(5)==0xA1 && b.read_register(5)==0xB2 && c.read_register(5)==0xC3);
    Ads7066FaultOverlay overlay(a); const auto t=a.now_ns(); overlay.schedule({t+100,FaultKind::ConversionCode,0,0x5555}); overlay.advance_time(100); CHECK(overlay.pending_events()==0);
    overlay.set_cs(false); overlay.schedule({a.now_ns(),FaultKind::ForceSdoHigh,0,0}); CHECK(overlay.clock_spi_bit(false)==PinLevel::High); overlay.schedule({a.now_ns(),FaultKind::ReleaseSdo,0,0}); (void)overlay.clock_spi_bit(false);
    overlay.set_cs(true); const auto monotonic=a.now_ns(); overlay.schedule({monotonic,FaultKind::Brownout,0,0}); overlay.advance_time(0); CHECK(a.now_ns()==monotonic && !a.responsive()); overlay.advance_time(5000000); CHECK(a.responsive());
}

using AtomicCase=std::pair<std::string,std::function<void()>>;
std::vector<AtomicCase> atomic_cases() {
    std::vector<AtomicCase> cases{
        {"ADS-DIG-001",[]{Ads7066Model d;ready(d);CHECK(d.read_register(0)==0x81);}},
        {"ADS-DIG-002",[]{Ads7066Model d;ready(d);CHECK(d.read_register(4)==0x04);}},
        {"ADS-DIG-003",[]{Ads7066Model d;ready(d);write_reg(d,0,1);CHECK((d.read_register(0)&1u)==0);write_reg(d,1,0x8E);write_reg(d,0xBF,0x96);for(uint8_t a:{uint8_t{2},uint8_t{3},uint8_t{4},uint8_t{5},uint8_t{7},uint8_t{9},uint8_t{0x0B},uint8_t{0x11},uint8_t{0x12},uint8_t{0xC0},uint8_t{0xC1},uint8_t{0xC2}})write_reg(d,a,0xFF);write_reg(d,0x10,0x11);CHECK((d.read_register(0)&0x40u)!=0&&d.conversion_pending());write_reg(d,1,1);d.advance_time(5000000);const std::array<std::pair<uint8_t,uint8_t>,17> reset{{{0,0x81},{1,0},{2,0},{3,0},{4,4},{5,0},{7,0},{9,0},{0x0B,0},{0x0D,0},{0x10,0},{0x11,0},{0x12,0},{0xBF,0},{0xC0,0},{0xC1,0},{0xC2,0}}};for(const auto& item:reset)CHECK(d.read_register(item.first)==item.second);CHECK(!d.conversion_pending()&&d.premature_reads()==0&&d.invalid_frames().empty()&&d.reserved_accesses().empty()&&d.sdo()==PinLevel::HighZ);}},
        {"ADS-DIG-004",[]{Ads7066Model d;ready(d);d.power_off();CHECK(d.sdo()==PinLevel::HighZ);}},
        {"ADS-DIG-005",[]{Ads7066Model d;ready(d);CHECK(d.sdo()==PinLevel::HighZ);d.set_cs(false);CHECK(d.sdo()!=PinLevel::HighZ);d.set_cs(true);CHECK(d.sdo()==PinLevel::HighZ);}},
        {"ADS-DIG-006",[]{Ads7066Model d;ready(d);d.set_cs(false);for(uint8_t byte:{uint8_t{8},uint8_t{5},uint8_t{0xA5}})for(int bit=7;bit>=0;--bit)(void)d.clock_spi_bit((byte&(1u<<bit))!=0u);CHECK(d.read_register(5)==0);d.set_cs(true);CHECK(d.read_register(5)==0xA5);}},
        {"ADS-DIG-007",[]{Ads7066Model d;ready(d);write_reg(d,5,0x5A);(void)frame(d,{0,0,0});CHECK(d.read_register(5)==0x5A);}},
        {"ADS-DIG-008",[]{Ads7066Model d;ready(d);write_reg(d,5,0xA5);CHECK(d.read_register(5)==0xA5);write_reg(d,0x11,0xFF);CHECK(d.read_register(0x11)==0x0F);}},
        {"ADS-DIG-009",[]{Ads7066Model d;ready(d);write_reg(d,5,0xA5);const auto first=frame(d,{0x10,5,0});CHECK(first[0]!=0xA5);const auto second=frame(d,{0,0,0});CHECK(second[0]==0xA5);}},
        {"ADS-DIG-010",[]{Ads7066Model d;ready(d);write_reg(d,5,0x24);write_reg(d,5,0x81,0x18);CHECK(d.read_register(5)==0xA5);}},
        {"ADS-DIG-011",[]{Ads7066Model d;ready(d);write_reg(d,5,0xA5);write_reg(d,5,0x81,0x20);CHECK(d.read_register(5)==0x24);}},
        {"ADS-DIG-012",[]{Ads7066Model d;ready(d);const std::array<uint8_t,5> opcodes{{0,8,0x10,0x18,0x20}};for(unsigned op=0;op<256;++op){bool valid=false;for(uint8_t x:opcodes)valid|=op==x;if(!valid){(void)frame(d,{static_cast<uint8_t>(op),5,0xA5});CHECK(d.read_register(5)==0);}}const std::array<uint8_t,17> addresses{{0,1,2,3,4,5,7,9,0x0B,0x0D,0x10,0x11,0x12,0xBF,0xC0,0xC1,0xC2}};for(unsigned a=0;a<256;++a){bool valid=false;for(uint8_t x:addresses)valid|=a==x;if(!valid){write_reg(d,static_cast<uint8_t>(a),0xFF);CHECK(d.read_register(static_cast<uint8_t>(a))==0);}}}},
        {"ADS-DIG-013",[]{Ads7066Model d;ready(d);write_reg(d,0,0xFF);CHECK(d.read_register(0)==0x80);const std::array<std::pair<uint8_t,uint8_t>,10> masks{{{2,0xB3},{3,0x07},{4,0x1F},{5,0xFF},{7,0xFF},{9,0xFF},{0x0B,0xFF},{0x10,0x13},{0x11,0x0F},{0x12,0xFF}}};for(const auto& item:masks){write_reg(d,item.first,0xFF);CHECK(d.read_register(item.first)==item.second);}write_reg(d,0x0D,0xFF);CHECK(d.read_register(0x0D)==0);write_reg(d,0xBF,0x96);write_reg(d,0xC0,0xFF);write_reg(d,0xC1,0xFF);write_reg(d,0xC2,0xFF);CHECK(d.read_register(0xBF)==0x96&&d.read_register(0xC0)==0x11&&d.read_register(0xC1)==0xFF&&d.read_register(0xC2)==0xFF);write_reg(d,1,0xFE);CHECK(d.read_register(1)==0xCE);}},
        {"ADS-DIG-014",[]{Ads7066Model d;ready(d);CHECK((d.read_register(0)&1u)!=0);write_reg(d,0,1);CHECK((d.read_register(0)&1u)==0);write_reg(d,1,0x40);const uint8_t bad[4]{8,5,1,0};(void)frame(d,{bad[0],bad[1],bad[2],bad[3]});CHECK((d.read_register(0)&2u)!=0);const uint8_t clear[3]{8,0,2};(void)frame(d,{clear[0],clear[1],clear[2],golden_crc8(clear,3)});CHECK((d.read_register(0)&2u)==0);}},
        {"ADS-DIG-015",[]{Ads7066Model d;ready(d);write_reg(d,0x12,1);write_reg(d,0x10,0x11);CHECK((d.read_register(0)&0x40u)!=0);write_reg(d,0x10,0);CHECK((d.read_register(0)&0x40u)==0);}},
        {"ADS-DIG-016",[]{Ads7066Model d;ready(d);d.set_calibration_duration(100);write_reg(d,1,2);CHECK((d.read_register(1)&2u)!=0);d.advance_time(99);CHECK((d.read_register(1)&2u)!=0);d.advance_time(1);CHECK((d.read_register(1)&2u)==0);}},
        {"ADS-DIG-017",[]{Ads7066Model d;ready(d);for(uint8_t pin=0;pin<8;++pin)d.set_gpio_input(pin,true);write_reg(d,5,0xFF);write_reg(d,7,0x55);write_reg(d,9,0x55);write_reg(d,0x0B,0x55);CHECK(read_reg(d,0x0D)==0xAA);for(uint8_t pin=0;pin<8;pin+=2)CHECK(d.gpio_drive(pin)==PinLevel::High);write_reg(d,1,4);CHECK(d.read_register(5)==0xFF);for(uint8_t pin=0;pin<8;++pin)CHECK(d.gpio_drive(pin)==PinLevel::HighZ);CHECK(read_reg(d,0x0D)==0);}},
        {"ADS-DIG-018",[]{Ads7066Model d;ready(d);write_reg(d,2,0x80);for(unsigned n=0;n<2;++n){const auto rx=frame(d,{0,0,0});CHECK(rx.size()==3&&rx[0]==0xA5&&rx[1]==0xA5&&rx[2]==0);}}},
        {"ADS-DIG-019",[]{for(uint8_t mode=0;mode<4;++mode)spi_mode_case(mode);}},
        {"ADS-DIG-020",[]{for(uint8_t osr=0;osr<8;++osr){Ads7066Model d;ready(d);write_reg(d,3,osr);const uint64_t count=uint64_t{1}<<osr;d.queue_analog_code(0,1,d.now_ns());for(uint64_t i=1;i<=count;++i)d.queue_analog_code(0,static_cast<uint32_t>(i+1u),d.now_ns()+4000u*i);(void)frame(d,{0,0,0});const uint64_t duration=osr==0?3200u:4000u*count*106u/100u;d.advance_time(duration);const auto rx=frame(d,{0,0,0});if(osr==0)CHECK(rx[0]==0&&rx[1]==1);else{const uint32_t raw=(uint32_t{rx[0]}<<12u)|(uint32_t{rx[1]}<<4u)|(rx[2]>>4u);const uint64_t sum=count*(count+1u)/2u;CHECK(raw==(sum<<4u)/count);}}}},
        {"ADS-DIG-021",[]{Ads7066Model d;ready(d);write_reg(d,3,2);d.queue_analog_code(0,0x1000,d.now_ns());d.queue_analog_code(0,0x2000,d.now_ns()+4000);d.queue_analog_code(0,0x3000,d.now_ns()+8000);d.queue_analog_code(0,0x4000,d.now_ns()+12000);(void)frame(d,{0,0,0});d.advance_time(16960);const auto rx=frame(d,{0,0,0});const uint32_t raw=(uint32_t{rx[0]}<<12u)|(uint32_t{rx[1]}<<4u)|(rx[2]>>4u);CHECK(raw==0x28000u);}},
        {"ADS-DIG-023",[]{for(uint8_t pin=0;pin<8;++pin){Ads7066Model d;ready(d);const uint8_t bit=static_cast<uint8_t>(1u<<pin);write_reg(d,7,bit);write_reg(d,9,bit);write_reg(d,0x0B,bit);CHECK(d.gpio_drive(pin)==PinLevel::HighZ);write_reg(d,5,bit);CHECK(d.gpio_drive(pin)==PinLevel::High);}}},
        {"ADS-DIG-024",[]{for(uint8_t pin=0;pin<8;++pin){Ads7066Model d;ready(d);const uint8_t bit=static_cast<uint8_t>(1u<<pin);write_reg(d,5,bit);d.set_gpio_input(pin,true);CHECK((read_reg(d,0x0D)&bit)!=0&&d.gpio_drive(pin)==PinLevel::HighZ);write_reg(d,7,bit);CHECK((read_reg(d,0x0D)&bit)==0);}}},
        {"ADS-DIG-025",[]{for(uint8_t pin=0;pin<8;++pin){Ads7066Model d;ready(d);const uint8_t bit=static_cast<uint8_t>(1u<<pin);write_reg(d,5,bit);write_reg(d,7,bit);write_reg(d,0x0B,bit);CHECK(d.gpio_drive(pin)==PinLevel::HighZ);write_reg(d,9,bit);CHECK(d.gpio_drive(pin)==PinLevel::High);}}},
        {"ADS-DIG-026",[]{for(uint8_t pin=0;pin<8;++pin){Ads7066Model d;ready(d);const uint8_t bit=static_cast<uint8_t>(1u<<pin);write_reg(d,5,bit);write_reg(d,7,bit);write_reg(d,9,bit);write_reg(d,0x0B,0);CHECK(d.gpio_drive(pin)==PinLevel::Low);write_reg(d,0x0B,bit);CHECK(d.gpio_drive(pin)==PinLevel::High);}}},
        {"ADS-DIG-027",[]{Ads7066Model d;ready(d);for(uint8_t pin=0;pin<8;++pin)d.set_gpio_input(pin,true);CHECK(read_reg(d,0x0D)==0);write_reg(d,5,0xFF);CHECK(read_reg(d,0x0D)==0xFF);write_reg(d,7,0x55);CHECK(read_reg(d,0x0D)==0xAA);}},
        {"ADS-DIG-028",[]{Ads7066Model d;ready(d);d.set_analog_code(0,0x1000);d.set_analog_code(2,0x2000);write_reg(d,0x11,2);d.advance_time(3200);const auto old_channel=frame(d,{0,0,0});CHECK(old_channel[0]==0x10&&old_channel[1]==0);d.advance_time(3200);const auto selected=frame(d,{0,0,0});CHECK(selected[0]==0x20&&selected[1]==0);}},
        {"ADS-DIG-029",[]{Ads7066Model d;ready(d);d.set_analog_code(5,0x5A5A);write_reg(d,0x10,2);d.set_cs(false);for(uint8_t x:{uint8_t{0xA8},uint8_t{0}})for(int bit=7;bit>=0;--bit)(void)d.clock_spi_bit((x&(1u<<bit))!=0u);d.set_cs(true);d.advance_time(3200);const auto rx=frame(d,{0,0});CHECK(rx[0]==0x5A&&rx[1]==0x5A);}},
        {"ADS-DIG-030",[]{Ads7066Model d;ready(d);d.set_analog_code(0,0x1000);d.set_analog_code(2,0x2000);d.set_analog_code(5,0x5000);write_reg(d,0x12,0x25);write_reg(d,0x10,0x11);(void)frame(d,{0,0,0});const std::array<uint8_t,4> expected{{0x10,0x20,0x50,0x10}};for(uint8_t value:expected){d.advance_time(3200);const auto rx=frame(d,{0,0,0});CHECK(rx[0]==value);}}},
        {"ADS-DIG-031",[]{for(uint8_t mode:{uint8_t{0},uint8_t{1},uint8_t{2},uint8_t{3}}){Ads7066Model d;ready(d);write_reg(d,0x12,1);write_reg(d,0x10,static_cast<uint8_t>(0x10u|mode));CHECK(((d.read_register(0)&0x40u)!=0u)==(mode==1));write_reg(d,0x10,mode);CHECK((d.read_register(0)&0x40u)==0);}}},
        {"ADS-DIG-032",[]{Ads7066Model d;ready(d);write_reg(d,0x12,0);write_reg(d,0x10,0x11);CHECK((d.read_register(0)&0x40u)==0);}},
        {"ADS-DIG-033",[]{Ads7066Model d;ready(d);d.set_analog_code(0,0xABCD);(void)frame(d,{0,0,0});d.advance_time(3200);const auto rx=frame(d,{0,0,0});CHECK(rx.size()==3&&rx[0]==0xAB&&rx[1]==0xCD&&rx[2]==0);}},
        {"ADS-DIG-034",[]{Ads7066Model d;ready(d);d.set_analog_code(0,0xABCD);write_reg(d,3,1);(void)frame(d,{0,0,0});d.advance_time(8480);const auto rx=frame(d,{0,0,0});CHECK(rx.size()==3&&rx[0]==0xAB&&rx[1]==0xCD&&rx[2]==0);}},
        {"ADS-DIG-035",[]{Ads7066Model d;ready(d);d.set_analog_code(5,0x1234);write_reg(d,0x11,5);write_reg(d,2,0x10);(void)frame(d,{0,0,0});d.advance_time(3200);const auto normal=frame(d,{0,0,0});CHECK(normal[0]==0x12&&normal[1]==0x34&&normal[2]==0x50);write_reg(d,3,1);d.advance_time(8480);(void)frame(d,{0,0,0});d.advance_time(8480);const auto averaged=frame(d,{0,0,0});CHECK((averaged[2]&0x0Fu)==5);}},
        {"ADS-DIG-036",[]{Ads7066Model normal;ready(normal);write_reg(normal,2,0x20);const auto base=frame(normal,{0,0,0});CHECK(base[2]==0x80);Ads7066Model diag;ready(diag);write_reg(diag,2,0x20);write_reg(diag,0xBF,0x96);write_reg(diag,0xC0,1);(void)frame(diag,{0,0,0});diag.advance_time(3200);const auto diag_rx=frame(diag,{0,0,0});CHECK(diag_rx[2]==0x90);Ads7066Model vtest;ready(vtest);vtest.set_vtest_code(0x7777);write_reg(vtest,0x11,6);write_reg(vtest,2,0x20);write_reg(vtest,0xBF,0x96);write_reg(vtest,0xC0,0x10);(void)frame(vtest,{0,0,0});vtest.advance_time(3200);const auto vtest_rx=frame(vtest,{0,0,0});CHECK(vtest_rx[2]==0xC0);Ads7066Model crc;ready(crc);write_reg(crc,2,0x20);write_reg(crc,1,0x40);(void)frame(crc,{0,0,0,0});const uint8_t nop[3]{0,0,0};(void)frame(crc,{0,0,0,golden_crc8(nop,3)});crc.advance_time(3200);const auto crc_rx=frame(crc,{0,0,0,golden_crc8(nop,3)});CHECK(crc_rx[2]==0xA0);}},
        {"ADS-DIG-037",[]{const std::array<std::array<uint8_t,4>,8> vectors{{{{0,0,0,0x2B}},{{8,1,0x48,0x90}},{{8,0,2,0x74}},{{0xFF,0xFF,0xFF,0x24}},{{0x12,0x34,0x56,0x57}},{{0xA5,0xA5,0,0xFA}},{{0x10,5,0,0xC8}},{{0x08,5,0xA5,0x49}}}};for(const auto& v:vectors){CHECK(golden_crc8(v.data(),3)==v[3]);CHECK(Ads7066Model::crc8(v.data(),3)==v[3]);}}},
        {"ADS-DIG-038",[]{Ads7066Model d;ready(d);write_reg(d,1,0x40);const uint8_t cmd[3]{8,5,0xA5};(void)frame(d,{cmd[0],cmd[1],cmd[2],golden_crc8(cmd,3)});CHECK(d.read_register(5)==0xA5);d.set_cs(false);for(uint8_t x:{uint8_t{8},uint8_t{5},uint8_t{0x5A}})for(int bit=7;bit>=0;--bit)(void)d.clock_spi_bit((x&(1u<<bit))!=0u);d.set_cs(true);CHECK(d.read_register(5)==0xA5&&d.invalid_frames().back()==24);}},
        {"ADS-DIG-039",[]{Ads7066Model d;ready(d);write_reg(d,1,0x40);const auto before=d.read_register(5);(void)frame(d,{8,5,0xA5,0});CHECK(d.read_register(5)==before&&(d.read_register(0)&2u)!=0);}},
        {"ADS-DIG-040",[]{Ads7066Model d;ready(d);write_reg(d,1,0x40);(void)frame(d,{8,5,0xA5,0});const uint8_t blocked[3]{8,5,0x5A};(void)frame(d,{blocked[0],blocked[1],blocked[2],golden_crc8(blocked,3)});CHECK(d.read_register(5)==0);const uint8_t general[3]{8,1,0x44};(void)frame(d,{general[0],general[1],general[2],golden_crc8(general,3)});CHECK((d.read_register(1)&0x44u)==0x44u);const uint8_t clear[3]{8,0,2};(void)frame(d,{clear[0],clear[1],clear[2],golden_crc8(clear,3)});CHECK((d.read_register(0)&2u)==0);}},
        {"ADS-DIG-041",[]{Ads7066Model d;ready(d);write_reg(d,5,0xA5);write_reg(d,1,0x40);const uint8_t rd[3]{0x10,5,0};(void)frame(d,{rd[0],rd[1],rd[2],golden_crc8(rd,3)});const uint8_t nop[3]{0,0,0};const auto rx=frame(d,{0,0,0,golden_crc8(nop,3)});CHECK(rx[0]==0xA5&&rx[1]==golden_crc8(rx.data(),1)&&rx[2]==0&&rx[3]==0);}},
        {"ADS-DIG-042",[]{Ads7066Model a;ready(a);a.set_analog_code(0,0x1234);write_reg(a,1,0x40);const uint8_t nop[3]{0,0,0};(void)frame(a,{0,0,0,golden_crc8(nop,3)});a.advance_time(3200);const auto two=frame(a,{0,0,0,golden_crc8(nop,3)});CHECK(two[0]==0x12&&two[1]==0x34&&two[2]==golden_crc8(two.data(),2)&&two[3]==0);Ads7066Model b;ready(b);b.set_analog_code(0,0x1234);write_reg(b,2,0x20);write_reg(b,1,0x40);(void)frame(b,{0,0,0,golden_crc8(nop,3)});b.advance_time(3200);const auto three=frame(b,{0,0,0,golden_crc8(nop,3)});CHECK(three[0]==0x12&&three[1]==0x34&&three[2]==0x80&&three[3]==golden_crc8(three.data(),3));}},
        {"ADS-DIG-043",[]{Ads7066Model d;ready(d);for(uint8_t a:{uint8_t{0xC0},uint8_t{0xC1},uint8_t{0xC2}}){write_reg(d,a,0xFF);CHECK(d.read_register(a)==0);}write_reg(d,0xBF,0x95);write_reg(d,0xC1,0xA5);CHECK(d.read_register(0xC1)==0);write_reg(d,0xBF,0x96);write_reg(d,0xC0,0x11);write_reg(d,0xC1,0xA5);write_reg(d,0xC2,0x5A);CHECK(d.read_register(0xC0)==0x11&&d.read_register(0xC1)==0xA5&&d.read_register(0xC2)==0x5A);}},
        {"ADS-DIG-044",[]{Ads7066Model d;ready(d);d.set_vtest_code(0x7777);write_reg(d,0x11,6);write_reg(d,2,0x20);write_reg(d,0xBF,0x96);write_reg(d,0xC1,0x34);write_reg(d,0xC2,0x12);write_reg(d,0xC0,0x11);(void)frame(d,{0,0,0});d.advance_time(3200);const auto bitwalk=frame(d,{0,0,0});CHECK(bitwalk[0]==0x12&&bitwalk[1]==0x34&&bitwalk[2]==0x90);write_reg(d,0xC0,0x10);d.advance_time(3200);(void)frame(d,{0,0,0});d.advance_time(3200);const auto vtest=frame(d,{0,0,0});CHECK(vtest[0]==0x77&&vtest[1]==0x77&&vtest[2]==0xC0);}},
        {"ADS-DIG-045",[]{Ads7066Model d;ready(d);write_reg(d,0xBF,0x96);write_reg(d,0xC1,0x34);write_reg(d,0xC2,0x12);write_reg(d,0xC0,1);(void)frame(d,{0,0,0});d.advance_time(3200);const auto rx=frame(d,{0,0,0});CHECK(rx[0]==0x12&&rx[1]==0x34);}},
        {"ADS-DIG-046",[]{Ads7066Model d;ready(d);d.set_vtest_code(0x7777);write_reg(d,0x11,6);write_reg(d,0xBF,0x96);write_reg(d,0xC0,0x10);(void)frame(d,{0,0,0});d.advance_time(3200);const auto rx=frame(d,{0,0,0});CHECK(rx[0]==0x77&&rx[1]==0x77);}},
        {"ADS-DIG-047",[]{Ads7066Model d;ready(d);for(unsigned clocks:{16u,20u}){d.set_cs(false);for(unsigned i=0;i<clocks;++i)(void)d.clock_spi_bit(false);d.set_cs(true);CHECK(d.invalid_frames().empty());}d.set_cs(false);for(unsigned i=0;i<23;++i)(void)d.clock_spi_bit(i==4);d.set_cs(true);CHECK(d.read_register(5)==0&&d.invalid_frames().back()==23);}},
        {"ADS-DIG-048",[]{Ads7066Model a,b;ready(a);ready(b);Ads7066Bus bus({&a,&b});const auto t=a.now_ns();CHECK(bus.transfer({0,0,0}).result==BusResult::NoDeviceSelected&&a.now_ns()==t+480&&b.now_ns()==t+480);bus.set_cs(1,false);CHECK(bus.transfer({0,0,0}).result==BusResult::Ok);bus.set_cs(1,true);CHECK(b.conversion_pending());bus.set_cs(0,false);CHECK(bus.transfer({8,5,0xA5}).result==BusResult::Ok);bus.set_cs(0,true);CHECK(a.read_register(5)==0xA5&&b.read_register(5)==0&&a.now_ns()==b.now_ns());bus.set_cs(0,false);bus.set_cs(1,false);CHECK(bus.transfer({8,7,0x5A}).result==BusResult::Contention);bus.set_cs(0,true);bus.set_cs(1,true);CHECK(a.read_register(7)==0x5A&&b.read_register(7)==0x5A&&a.now_ns()==b.now_ns());}},
        {"ADS-DIG-049",[]{Ads7066Model a,b,c;ready(a);ready(b);ready(c);Ads7066DaisyChain chain({&a,&b,&c});chain.set_cs(false);const auto transfer=chain.transfer({8,5,0xC3,8,5,0xB2,8,5,0xA1});chain.set_cs(true);CHECK(transfer.result==BusResult::Ok&&a.read_register(5)==0xA1&&b.read_register(5)==0xB2&&c.read_register(5)==0xC3);}},
        {"ADS-DIG-050",[]{Ads7066Model a,b;ready(a);ready(b);{Ads7066DaisyChain chain({&a,&b});bool overlap=false;try{Ads7066DaisyChain forbidden({&b});}catch(const std::invalid_argument&){overlap=true;}CHECK(overlap);chain.set_cs(false);CHECK(chain.transfer({0,0,0,0,0,0}).result==BusResult::Ok);CHECK(!a.conversion_pending()&&!b.conversion_pending());chain.set_cs(true);CHECK(a.conversion_pending()&&b.conversion_pending()&&a.conversion_ready_ns()==b.conversion_ready_ns()&&a.now_ns()==b.now_ns());}CHECK(!a.daisy_chain_active()&&!b.daisy_chain_active());}},
        {"ADS-DIG-FLT-POWER",fault_power_case},
        {"ADS-DIG-FLT-BROWNOUT",fault_brownout_case},
        {"ADS-DIG-FLT-GPIO",fault_gpio_case},
        {"ADS-DIG-FLT-CODE",fault_code_boundary_case},
        {"ADS-DIG-FLT-DELAY",fault_delay_case},
        {"ADS-DIG-FLT-INPUT",fault_input_case},
        {"ADS-DIG-FLT-CRC",fault_crc_case},
        {"ADS-DIG-FLT-SDO",fault_sdo_case},
        {"ADS-DIG-FLT-VALIDATE",fault_validation_case},
        {"ADS-DIG-FLT-PRECEDENCE",fault_precedence_case},
        {"ADS-DIG-DAISY-CRC",daisy_crc_case},
        {"ADS-DIG-DAISY-DISABLED",daisy_disabled_case},
        {"ADS-DIG-FLT-ISOLATION",fault_isolation_case},
        {"ADS-DIG-053",[]{Ads7066Model d;ready(d);Ads7066FaultOverlay f(d);write_reg(d,3,2);d.set_analog_code(0,0x1000);const auto start=d.now_ns();(void)overlay_frame(f,{0,0,0});f.schedule({start+8000,FaultKind::ConversionCode,0,0x5000});f.advance_time(16960);const auto avg=overlay_frame(f,{0,0,0});const uint32_t raw=(uint32_t{avg[0]}<<12u)|(uint32_t{avg[1]}<<4u)|(avg[2]>>4u);CHECK(raw==0x30000u);write_reg(d,5,1);f.schedule({d.now_ns(),FaultKind::GpioLevel,0,1});f.advance_time(0);CHECK((read_reg(d,0x0D)&1u)!=0);write_reg(d,5,0);f.schedule({d.now_ns(),FaultKind::CorruptInputBit,0,0});(void)overlay_frame(f,{8,5,0xA5});CHECK(d.read_register(5)==0);f.set_cs(false);f.schedule({d.now_ns(),FaultKind::FlipSdoBit,0,0});const auto flipped=f.clock_spi_bit(false);CHECK(flipped!=d.sdo());f.schedule({d.now_ns(),FaultKind::ForceSdoHigh,0,0});CHECK(f.clock_spi_bit(false)==PinLevel::High);f.schedule({d.now_ns(),FaultKind::ReleaseSdo,0,0});(void)f.clock_spi_bit(false);f.set_cs(true);const uint64_t wide=uint64_t{1}<<32u;f.schedule({d.now_ns(),FaultKind::DelayReadiness,0,wide});(void)overlay_frame(f,{0,0,0});CHECK(d.conversion_ready_ns()-d.now_ns()>=wide);bool past=false,bad_index=false;try{f.schedule({d.now_ns()-1,FaultKind::Brownout,0,0});}catch(const std::invalid_argument&){past=true;}try{f.schedule({d.now_ns(),FaultKind::GpioLevel,8,1});}catch(const std::out_of_range&){bad_index=true;}CHECK(past&&bad_index);f.set_cs(false);f.schedule({d.now_ns(),FaultKind::PowerReset,0,0});f.advance_time(0);CHECK(!d.responsive()&&d.sdo()==PinLevel::HighZ);f.advance_time(5000000);CHECK(d.responsive()&&d.sdo()!=PinLevel::HighZ);f.set_cs(true);f.schedule({d.now_ns(),FaultKind::Brownout,0,0});f.advance_time(0);CHECK(!d.responsive());}},
        {"ADS-DIG-054",[]{Ads7066Model a;ready(a);Ads7066FaultOverlay fa(a);fa.set_cs(false);fa.schedule({a.now_ns(),FaultKind::ForceSdoHigh,0,0});fa.schedule({a.now_ns(),FaultKind::ReleaseSdo,0,0});fa.advance_time(0);CHECK(fa.clock_spi_bit(false)!=PinLevel::High);fa.set_cs(true);Ads7066Model b;ready(b);Ads7066FaultOverlay fb(b);fb.set_cs(false);fb.schedule({b.now_ns(),FaultKind::ReleaseSdo,0,0});fb.schedule({b.now_ns(),FaultKind::ForceSdoHigh,0,0});fb.advance_time(0);CHECK(fb.clock_spi_bit(false)==PinLevel::High);}},
        {"ADS-DIG-T74-01",[]{table_7_4_case(false,false,0,true);table_7_4_case(false,false,3,true);}},
        {"ADS-DIG-T74-02",[]{table_7_4_case(false,false,1,true);}},
        {"ADS-DIG-T74-03",[]{table_7_4_case(false,false,2,true);}},
        {"ADS-DIG-T74-04",[]{table_7_4_case(false,true,0,true);table_7_4_case(false,true,3,true);}},
        {"ADS-DIG-T74-05",[]{table_7_4_case(false,true,1,true);}},
        {"ADS-DIG-T74-06",[]{table_7_4_case(false,true,2,true);}},
        {"ADS-DIG-T74-07",[]{table_7_4_case(true,false,0,true);table_7_4_case(true,false,3,true);}},
        {"ADS-DIG-T74-08",[]{table_7_4_case(true,false,1,false);}},
        {"ADS-DIG-T74-09",[]{table_7_4_case(true,false,2,false);}},
        {"ADS-DIG-T74-10",[]{table_7_4_case(true,true,0,false);table_7_4_case(true,true,3,false);}},
        {"ADS-DIG-T74-11",[]{table_7_4_case(true,true,1,false);}},
        {"ADS-DIG-T74-12",[]{table_7_4_case(true,true,2,false);}},
        {"ADS-DIG-TPU",[]{Ads7066Model d;d.advance_time(4999999);CHECK(!d.responsive());d.advance_time(1);CHECK(d.responsive());}},
        {"ADS-DIG-TRST",[]{Ads7066Model d;ready(d);write_reg(d,1,1);d.advance_time(4999999);CHECK(!d.responsive());d.advance_time(1);CHECK(d.responsive()&&d.read_register(0)==0x81);}},
        {"ADS-DIG-TCONV",[]{Ads7066Model boundary;ready(boundary);boundary.set_analog_code(0,0xBEEF);(void)frame(boundary,{0,0,0});const auto due=boundary.conversion_ready_ns();CHECK(due-boundary.now_ns()==3200);boundary.advance_time(3199);CHECK(boundary.conversion_pending());boundary.advance_time(1);CHECK(!boundary.conversion_pending());const auto done=frame(boundary,{0,0,0});CHECK(done[0]==0xBE&&done[1]==0xEF);Ads7066Model early;ready(early);(void)frame(early,{0,0,0});(void)frame(early,{0,0,0});CHECK(early.premature_reads()==1);}},
        {"ADS-DIG-CONV-CS",[]{Ads7066Model d;ready(d);d.set_cs(false);for(unsigned i=0;i<24;++i)(void)d.clock_spi_bit(false);CHECK(!d.conversion_pending());d.set_cs(true);CHECK(d.conversion_pending());}}
        ,{"ADS-DIG-PROP",[]{uint32_t state=0x7066C0DEu;const auto next=[&state](){state^=state<<13;state^=state>>17;state^=state<<5;return state;};Ads7066Model d;ready(d);const std::array<std::pair<uint8_t,uint8_t>,6> regs{{{5,0xFF},{7,0xFF},{9,0xFF},{0x0B,0xFF},{0x11,0x0F},{0x12,0xFF}}};std::array<uint8_t,6> expected{};for(unsigned i=0;i<512;++i){const std::size_t index=next()%regs.size();const uint8_t value=static_cast<uint8_t>(next());const uint8_t op=std::array<uint8_t,3>{{0x08,0x18,0x20}}[next()%3];if(op==0x08)expected[index]=value&regs[index].second;else if(op==0x18)expected[index]|=value&regs[index].second;else expected[index]&=static_cast<uint8_t>(~value);write_reg(d,regs[index].first,value,op);CHECK(d.read_register(regs[index].first)==expected[index]);CHECK(d.responsive()&&d.sdo()==PinLevel::HighZ&&d.reserved_accesses().empty());}std::cout<<"PASS ADS-PROPERTY seed=0x7066C0DE commands=512 register_mask_cs_responsive_invariants\n";}}
        ,{"ADS-DIG-TRANS",[]{
            { Ads7066Model d;ready(d);write_reg(d,0x11,3);write_reg(d,0x10,2);CHECK((d.read_register(0x10)&3u)==2u);d.set_cs(false);for(uint8_t x:{uint8_t{0xA8},uint8_t{0}})for(int bit=7;bit>=0;--bit)(void)d.clock_spi_bit((x&(1u<<bit))!=0u);d.set_cs(true);CHECK(d.conversion_pending()); }
            { Ads7066Model d;ready(d);write_reg(d,0x12,0x05);write_reg(d,0x10,0x11);CHECK((d.read_register(0)&0x40u)!=0);write_reg(d,0x10,0);CHECK((d.read_register(0)&0x40u)==0&&(d.read_register(0x10)&3u)==0); }
            { Ads7066Model d;ready(d);write_reg(d,0x12,0x05);write_reg(d,0x10,0x11);write_reg(d,0x10,1);CHECK((d.read_register(0)&0x40u)==0);write_reg(d,0x10,0x11);CHECK((d.read_register(0)&0x40u)!=0); }
            { Ads7066Model d;ready(d);write_reg(d,5,0xA5);write_reg(d,1,1);d.advance_time(5000000);CHECK(d.read_register(5)==0&&d.read_register(0x10)==0);write_reg(d,0x12,1);write_reg(d,0x10,0x11);CHECK((d.read_register(0)&0x40u)!=0); }
            { Ads7066Model d;ready(d);write_reg(d,5,0xA5);d.brownout();CHECK(!d.responsive()&&d.read_register(5)==0);d.advance_time(4999999);CHECK(!d.responsive());d.advance_time(1);CHECK(d.responsive());write_reg(d,0x12,1);write_reg(d,0x10,0x11);CHECK((d.read_register(0)&0x40u)!=0); }
            { Ads7066Model d;ready(d);write_reg(d,0x12,1);write_reg(d,0x10,0x11);(void)frame(d,{0,0,0});CHECK(d.conversion_pending());const auto due=d.conversion_ready_ns();write_reg(d,0x12,4);CHECK(d.conversion_pending()&&d.conversion_ready_ns()>=due&&d.read_register(0x12)==4&&(d.read_register(0)&0x40u)!=0); }
        }}
        ,{"ADS-DIG-RESET-EQUIV",[]{const auto dirty=[](Ads7066Model& d){ready(d);write_reg(d,5,0xA5);write_reg(d,7,0x5A);write_reg(d,0x12,0x81);write_reg(d,0x10,0x11);};Ads7066Model software,power,brown;dirty(software);dirty(power);dirty(brown);write_reg(software,1,1);software.advance_time(5000000);power.power_off();power.power_on();power.advance_time(5000000);brown.brownout();brown.advance_time(5000000);const std::array<uint8_t,13> regs{{0,1,2,3,4,5,7,9,0x0B,0x10,0x11,0x12,0xBF}};for(uint8_t reg:regs)CHECK(software.read_register(reg)==power.read_register(reg)&&power.read_register(reg)==brown.read_register(reg));CHECK(software.responsive()&&power.responsive()&&brown.responsive());CHECK(!software.conversion_pending()&&!power.conversion_pending()&&!brown.conversion_pending());CHECK(software.sdo()==PinLevel::HighZ&&power.sdo()==PinLevel::HighZ&&brown.sdo()==PinLevel::HighZ);}}
        ,{"ADS-DIG-TRACE-PINS",[]{Ads7066Model d;CHECK(!d.responsive()&&d.sdo()==PinLevel::HighZ);std::cout<<"TRACE ADS-TRACE-CS-SDO t_ns="<<d.now_ns()<<" cs=1 sdo=Z responsive=0 conversion=0\n";ready(d);CHECK(d.sdo()==PinLevel::HighZ);std::cout<<"TRACE ADS-TRACE-CS-SDO t_ns="<<d.now_ns()<<" cs=1 sdo=Z responsive=1 conversion=0\n";d.set_cs(false);CHECK(d.sdo()!=PinLevel::HighZ);std::cout<<"TRACE ADS-TRACE-SPI-EDGES t_ns="<<d.now_ns()<<" cs=0 sclk=0 sdi=0 sdo="<<(d.sdo()==PinLevel::High?1:0)<<" conversion=0\n";(void)d.clock_edge(true,false);std::cout<<"TRACE ADS-TRACE-SPI-EDGES t_ns="<<d.now_ns()<<" cs=0 sclk=1 sdi=0 sdo="<<(d.sdo()==PinLevel::High?1:0)<<" conversion=0\n";(void)d.clock_edge(false,false);d.set_cs(true);CHECK(d.sdo()==PinLevel::HighZ&&d.conversion_pending());std::cout<<"TRACE ADS-TRACE-CONV-CS t_ns="<<d.now_ns()<<" cs=1 sdo=Z conversion=1 ready_ns="<<d.conversion_ready_ns()<<"\n";}}
    };
    static constexpr std::array<uint64_t,16> high_us{{0,0,0,0,4,6,8,12,16,24,32,48,64,96,128,192}};
    static constexpr std::array<uint64_t,16> low_us{{32,48,64,96,128,192,256,384,512,768,1024,1536,2048,3072,4096,6144}};
    for(auto& entry:cases)if(entry.first=="ADS-DIG-053"||entry.first=="ADS-DIG-054")entry.first="MODEL-PARTIAL-"+entry.first;
    for(uint8_t code=4;code<16;++code){const auto ns=high_us[code]*1000u;cases.emplace_back("ADS-DIG-T73-H"+(code<10?std::string("0"):std::string())+std::to_string(code),[code,ns]{table_7_3_case(code,ns);});}
    for(uint8_t code=0;code<16;++code){const auto ns=low_us[code]*1000u;cases.emplace_back("ADS-DIG-T73-L"+(code<10?std::string("0"):std::string())+std::to_string(code),[code,ns]{table_7_3_case(static_cast<uint8_t>(0x10u|code),ns);});}
    return cases;
}
}

int main() {
    for(const auto& entry:atomic_cases()){try{entry.second();std::cout<<"PASS "<<entry.first<<'\n';}catch(const std::exception& e){++failures;std::cout<<"FAIL "<<entry.first<<' '<<e.what()<<'\n';}}
    const std::array<std::pair<const char*,std::function<void()>>,7> tests{{
        {"MODEL-ADS-RESET/PINS",reset_and_pin_conformance},{"MODEL-ADS-REG/CROSSREG",register_and_cross_register_conformance},
        {"MODEL-ADS-SPI/CRC",spi_frame_and_crc_conformance},{"MODEL-ADS-SPI-MODES",edge_mode_conformance},
        {"MODEL-ADS-TIME/OUTPUT",conversion_timing_and_output_conformance},{"MODEL-ADS-MODES/GPIO/DIAG",mode_gpio_diagnostic_conformance},
        {"MODEL-ADS-BUS/DAISY/FAULTS",bus_daisy_and_fault_conformance}
    }};
    for(const auto& entry:tests){try{entry.second();std::cout<<"PASS "<<entry.first<<'\n';}catch(const std::exception& e){++failures;std::cout<<"FAIL "<<entry.first<<' '<<e.what()<<'\n';}}
    return failures==0?EXIT_SUCCESS:EXIT_FAILURE;
}
