#include "ADS7066.hpp"
#include "fake_hal.hpp"
#include "sil_check.hpp"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
int failures = 0;
int known_defects = 0;
void xfail_if(bool observed, const char* id, const char* defect) {
    sil_check::xfail_if(observed, failures, known_defects, id, defect);
}

struct Fixture {
    SPIDriver spi{&hspi1, 100};
    IOPin cs1{&sil::gpio1, 1};
    IOPin cs2{&sil::gpio2, 2};
    ADS7066_channel_data data1{};
    ADS7066_channel_data data2{};
    ADS7066 adc1{spi, cs1, data1};
    ADS7066 adc2{spi, cs2, data2};
    Fixture() { sil::reset();sil::set_spi_bit_cell_ns(200);cs1.setPin(GPIO_PIN_SET);cs2.setPin(GPIO_PIN_SET);sil::gpio_events.clear(); }
};

void sil_spi_001_002() {
    sil::reset();
    SPIDriver spi{&hspi1, 100};
    IOPin cs{&sil::gpio1, 1};
    ADS7066_channel_data data{};
    ADS7066 adc{spi, cs, data};
    CHECK(sil::gpio_events.size() == 1);
    CHECK(sil::gpio_events[0].second == GPIO_PIN_SET);
    sil::gpio_events.clear();
    uint8_t tx[3]{1,2,3}, rx[3]{};
    CHECK(spi.transmit_Receive(tx, 3, rx, 2) == false);
    CHECK(sil::transfers.empty());
    sil::script_status(HAL_TIMEOUT);
    CHECK(adc.writeReg(1, static_cast<uint8_t>(2)) == false);
    CHECK(sil::gpio_events.size() == 2);
    CHECK(sil::gpio_events[0].second == GPIO_PIN_RESET);
    CHECK(sil::gpio_events[1].second == GPIO_PIN_SET);

    const auto check_null_failure = [](SilTransfer::Kind kind) {
        sil::reset();
        HAL_GPIO_WritePin(&sil::gpio1, 1, GPIO_PIN_RESET);
        sil::script_status(HAL_ERROR);
        HAL_StatusTypeDef status=HAL_OK;
        if (kind == SilTransfer::Kind::Transmit) status=HAL_SPI_Transmit(&hspi1,nullptr,3,9);
        else if (kind == SilTransfer::Kind::Receive) status=HAL_SPI_Receive(&hspi1,nullptr,3,9);
        else status=HAL_SPI_TransmitReceive(&hspi1,nullptr,nullptr,3,9);
        CHECK(status==HAL_ERROR);
        CHECK(sil::transfers.size()==1);
        CHECK(sil::transfers.front().mosi==std::vector<uint8_t>(3,0));
    };
    check_null_failure(SilTransfer::Kind::Transmit);
    check_null_failure(SilTransfer::Kind::Receive);
    check_null_failure(SilTransfer::Kind::TransmitReceive);

    // Current equal-length null/zero behavior is characterized explicitly;
    // the requirement owner still needs to designate reject vs HAL-delegate.
    sil::reset(); HAL_GPIO_WritePin(&sil::gpio1,1,GPIO_PIN_RESET);
    CHECK(spi.transmit_Receive(nullptr,0,nullptr,0));
    CHECK(sil::transfers.size()==1 && sil::transfers[0].mosi.empty());
    sil::reset(); HAL_GPIO_WritePin(&sil::gpio1,1,GPIO_PIN_RESET);
    CHECK(spi.transmit_Receive(nullptr,1,nullptr,1));
    CHECK(sil::transfers.size()==1 && sil::transfers[0].mosi==std::vector<uint8_t>(1,0));
    std::cout<<"BLOCKED SIL-SPI-002 equal_length_null_and_zero_length_contract_requires_owner_designation\n";
}

void sil_hal_wrapper_coverage() {
    sil::reset();
    IOPin default_constructed;
    (void)default_constructed; // constructor coverage; unusable until configured by design
    IOPin pin{&sil::gpio1,1};
    CHECK(pin.readPin()==GPIO_PIN_SET);
    pin.togglePin(); CHECK(pin.readPin()==GPIO_PIN_RESET);
    pin.togglePin(); CHECK(pin.readPin()==GPIO_PIN_SET);

    SPIDriver spi{&hspi1,37};
    uint8_t tx[3]{0x11,0x22,0x33},rx[3]{};
    HAL_GPIO_WritePin(&sil::gpio1,1,GPIO_PIN_RESET);
    CHECK(spi.transmit_Timeout(tx,3,41));
    CHECK(sil::transfers.back().kind==SilTransfer::Kind::Transmit && sil::transfers.back().timeout==41);
    CHECK(spi.receive(rx,3));
    CHECK(sil::transfers.back().kind==SilTransfer::Kind::Receive && sil::transfers.back().timeout==37);
    CHECK(spi.receive_Timeout(rx,3,43));
    CHECK(sil::transfers.back().kind==SilTransfer::Kind::Receive && sil::transfers.back().timeout==43);
    CHECK(spi.transmit_Receive_Timeout(tx,3,rx,3,47));
    CHECK(sil::transfers[sil::transfers.size()-2].kind==SilTransfer::Kind::Transmit);
    CHECK(sil::transfers[sil::transfers.size()-2].timeout==47);
    CHECK(sil::transfers.back().kind==SilTransfer::Kind::Receive && sil::transfers.back().timeout==47);
    sil::script_status(HAL_ERROR); sil::script_status(HAL_OK);
    CHECK(!spi.transmit_Receive_Timeout(tx,3,rx,3,53));
    CHECK(sil::transfers.back().kind==SilTransfer::Kind::Receive);
    sil::script_status(HAL_OK); sil::script_status(HAL_TIMEOUT);
    CHECK(!spi.transmit_Receive_Timeout(tx,3,rx,3,59));
}

void model_conformance_core() {
    Fixture f;
    CHECK(sil::ads_model(0).read_register(0x00) == 0x81);
    CHECK(sil::ads_model(0).read_register(0x04) == 0x04);
    f.cs1.setPin(GPIO_PIN_RESET);
    uint8_t set_cmd[3]{0x18, 0x05, 0xA5};
    uint8_t clr_cmd[3]{0x20, 0x05, 0x81};
    CHECK(f.spi.transmit(set_cmd, 3));
    CHECK(sil::ads_model(0).read_register(0x05) == 0); // decoded only when CS rises
    f.cs1.setPin(GPIO_PIN_SET);
    CHECK(sil::ads_model(0).read_register(0x05) == 0xA5);
    f.cs1.setPin(GPIO_PIN_RESET);
    CHECK(f.spi.transmit(clr_cmd, 3));
    CHECK(sil::ads_model(0).read_register(0x05) == 0xA5);
    f.cs1.setPin(GPIO_PIN_SET);
    CHECK(sil::ads_model(0).read_register(0x05) == 0x24);
    CHECK(sil::ads_model(1).read_register(0x05) == 0);
    f.cs1.setPin(GPIO_PIN_RESET);
    uint8_t masked[3]{0x08, 0x11, 0xFF};
    CHECK(f.spi.transmit(masked, 3)); f.cs1.setPin(GPIO_PIN_SET);
    CHECK(sil::ads_model(0).read_register(0x11) == 0x0F);
    f.cs1.setPin(GPIO_PIN_RESET);
    uint8_t reserved[3]{0x08, 0x06, 0xFF};
    CHECK(f.spi.transmit(reserved, 3)); f.cs1.setPin(GPIO_PIN_SET);
    CHECK(sil::ads_model(0).read_register(0x06) == 0);
    CHECK(sil::ads_model(0).reserved_accesses().size() == 1);

    sil::reset();
    HAL_GPIO_WritePin(&sil::gpio1,1,GPIO_PIN_RESET);
    HAL_GPIO_WritePin(&sil::gpio2,2,GPIO_PIN_RESET);
    uint8_t frame[3]{}; bool both_cs_rejected=false;
    try { (void)HAL_SPI_Transmit(&hspi1,frame,3,10); }
    catch(const std::runtime_error&) { both_cs_rejected=true; }
    CHECK(both_cs_rejected && sil::transfers.empty());

    GPIO_TypeDef unknown{99}; bool unknown_gpio_rejected=false;
    try { (void)HAL_GPIO_ReadPin(&unknown,1); }
    catch(const std::runtime_error&) { unknown_gpio_rejected=true; }
    CHECK(unknown_gpio_rejected);

    sil::reset();sil::set_spi_interframe_us(4);sil::set_sample(1,0,0xABCD);
    HAL_GPIO_WritePin(&sil::gpio1,1,GPIO_PIN_RESET);
    uint8_t response[3]{};
    CHECK(HAL_SPI_TransmitReceive(&hspi1,frame,response,3,10)==HAL_OK);
    CHECK(response[0]==0&&response[1]==0);
    HAL_GPIO_WritePin(&sil::gpio1,1,GPIO_PIN_SET);
    HAL_GPIO_WritePin(&sil::gpio2,2,GPIO_PIN_RESET);
    CHECK(HAL_SPI_TransmitReceive(&hspi1,frame,response,3,10)==HAL_OK);
    HAL_GPIO_WritePin(&sil::gpio2,2,GPIO_PIN_SET);
    HAL_GPIO_WritePin(&sil::gpio2,2,GPIO_PIN_RESET);
    CHECK(HAL_SPI_TransmitReceive(&hspi1,frame,response,3,10)==HAL_OK);
    CHECK(response[0]==0xAB && response[1]==0xCD && response[2]==0);
    HAL_GPIO_WritePin(&sil::gpio2,2,GPIO_PIN_SET);
    CHECK(sil::all_chip_selects_high());
    std::cout<<"PASS FAULT-CROSS-DEVICE isolated_detected_no_cross_data_safe_cs_queue_and_recovery\n";
}

void model_crc_conformance() {
    const auto reference_crc = [](const uint8_t* data, std::size_t size) {
        uint8_t crc = 0xFF;
        for (std::size_t n=0; n<size; ++n) {
            crc ^= data[n];
            for (int bit=0; bit<8; ++bit) crc = static_cast<uint8_t>((crc & 0x80u) ? ((crc << 1) ^ 0x07u) : (crc << 1));
        }
        return crc;
    };
    const std::array<std::array<uint8_t,4>,4> vectors{{{{0,0,0,0x2B}},{{0x08,0x01,0x48,0x90}},{{0x08,0,2,0x74}},{{0xFF,0xFF,0xFF,0x24}}}};
    for(const auto& v:vectors) { CHECK(reference_crc(v.data(),3)==v[3]); CHECK(sil::crc8_ccitt(v.data(),3)==v[3]); }
    Fixture f;
    uint8_t enable[3]{0x08,0x01,0x40};
    f.cs1.setPin(GPIO_PIN_RESET); CHECK(f.spi.transmit(enable,3)); f.cs1.setPin(GPIO_PIN_SET);
    CHECK((sil::ads_model(0).read_register(1) & 0x40u) != 0u);
    const uint8_t blocked_payload[3]{0x08,0x05,0xA5};
    uint8_t bad[4]{0x08,0x05,0xA5,0};
    f.cs1.setPin(GPIO_PIN_RESET); CHECK(f.spi.transmit(bad,4)); f.cs1.setPin(GPIO_PIN_SET);
    CHECK((sil::ads_model(0).read_register(0)&2u)!=0u && sil::ads_model(0).read_register(5)==0);
    uint8_t blocked[4]{0x08,0x05,0xA5,reference_crc(blocked_payload,3)};
    f.cs1.setPin(GPIO_PIN_RESET); CHECK(f.spi.transmit(blocked,4)); f.cs1.setPin(GPIO_PIN_SET);
    CHECK(sil::ads_model(0).read_register(5)==0);
    const uint8_t general_payload[3]{0x08,0x01,0x40};
    uint8_t general[4]{0x08,0x01,0x40,reference_crc(general_payload,3)};
    f.cs1.setPin(GPIO_PIN_RESET); CHECK(f.spi.transmit(general,4)); f.cs1.setPin(GPIO_PIN_SET);
    const uint8_t clear_payload[3]{0x08,0x00,0x02};
    uint8_t clear[4]{0x08,0x00,0x02,reference_crc(clear_payload,3)};
    f.cs1.setPin(GPIO_PIN_RESET); CHECK(f.spi.transmit(clear,4)); f.cs1.setPin(GPIO_PIN_SET);
    CHECK((sil::ads_model(0).read_register(0)&2u)==0u);
    f.cs1.setPin(GPIO_PIN_RESET); CHECK(f.spi.transmit(blocked,4)); f.cs1.setPin(GPIO_PIN_SET);
    CHECK(sil::ads_model(0).read_register(5)==0xA5);
    const uint8_t read_payload[3]{0x10,0x05,0};
    uint8_t read[4]{0x10,0x05,0,reference_crc(read_payload,3)}, rx[4]{};
    f.cs1.setPin(GPIO_PIN_RESET); CHECK(f.spi.transmit_Receive(read,4,rx,4)); f.cs1.setPin(GPIO_PIN_SET);
    const uint8_t nop_payload[3]{0,0,0};
    uint8_t nop[4]{0,0,0,reference_crc(nop_payload,3)};
    f.cs1.setPin(GPIO_PIN_RESET); CHECK(f.spi.transmit_Receive(nop,4,rx,4)); f.cs1.setPin(GPIO_PIN_SET);
    CHECK(rx[0]==0xA5); CHECK(rx[3]==reference_crc(rx,3));
    f.cs1.setPin(GPIO_PIN_RESET); CHECK(f.spi.transmit(bad,4)); f.cs1.setPin(GPIO_PIN_SET);
    CHECK((sil::ads_model(0).read_register(0)&2u)!=0u);
    f.cs1.setPin(GPIO_PIN_RESET); CHECK(f.spi.transmit_Receive(read,4,rx,4)); f.cs1.setPin(GPIO_PIN_SET);
    f.cs1.setPin(GPIO_PIN_RESET); CHECK(f.spi.transmit_Receive(nop,4,rx,4)); f.cs1.setPin(GPIO_PIN_SET);
    CHECK(rx[0]==0xA5 && rx[3]==reference_crc(rx,3));
}

void sil_spi_003_req_char() {
    Fixture f;
    uint8_t tx[3]{0x10, 0x01, 0}, rx[2]{0xA5, 0x5A};
    f.cs1.setPin(GPIO_PIN_RESET);
    const bool result = f.spi.transmit_Receive_Timeout(tx, 3, rx, 2, 77);
    f.cs1.setPin(GPIO_PIN_SET);
    const bool char_matches = result && sil::transfers.size() == 2
        && sil::transfers[0].kind == SilTransfer::Kind::Transmit
        && sil::transfers[1].kind == SilTransfer::Kind::Receive
        && sil::transfers[0].mosi.size() == 3 && sil::transfers[1].mosi.size() == 2;
    CHECK(char_matches);
    CHECK(sil::ads_model(0).invalid_frames() == std::vector<uint16_t>({40}));
    const bool req_matches = !result && sil::transfers.empty();
    xfail_if(!req_matches,"SIL-SPI-003","DEF-SPI-TIMEOUT-FULL-DUPLEX");
}

void sil_adc_001_002_isolation() {
    Fixture f;
    CHECK(f.adc1.writeReg(5, static_cast<uint8_t>(0xA5)));
    CHECK(sil::transfers.back().mosi == std::vector<uint8_t>({0x08, 0x05, 0xA5}));
    CHECK(sil::ads_model(0).read_register(5) == 0xA5);
    CHECK(sil::ads_model(1).read_register(5) == 0);
    uint8_t value = 0;
    CHECK(f.adc1.readReg(5, &value));
    CHECK(value == 0xA5);
    CHECK(sil::transfers[sil::transfers.size()-2].mosi == std::vector<uint8_t>({0x10,0x05,0}));
    CHECK(sil::transfers.back().mosi == std::vector<uint8_t>({0,0,0}));
}

void sil_adc_003_004() {
    Fixture f;
    CHECK(!f.adc1.readReg(0, nullptr));
    CHECK(!f.adc1.writeReg(0, static_cast<uint8_t*>(nullptr)));
    const auto count = sil::transfers.size();
    CHECK(!f.adc1.readChannel(8));
    CHECK(!f.adc1.readChannel(255));
    CHECK(sil::transfers.size() == count);
    uint8_t value{};
    for (const auto status : {HAL_ERROR,HAL_BUSY,HAL_TIMEOUT}) {
        for (std::size_t fail_at = 0; fail_at < 2; ++fail_at) {
            Fixture g;
            for (std::size_t i = 0; i < fail_at; ++i) sil::script_status(HAL_OK);
            sil::script_status(status);
            CHECK(!g.adc1.readReg(0, &value));
            CHECK(sil::transfers.size() == fail_at + 1);
            CHECK(HAL_GPIO_ReadPin(&sil::gpio1, 1) == GPIO_PIN_SET);
        }
        Fixture g; sil::script_status(status);
        CHECK(!g.adc1.writeReg(1,static_cast<uint8_t>(0x55)));
        CHECK(sil::ads_model(0).read_register(1)==0);
        CHECK(HAL_GPIO_ReadPin(&sil::gpio1,1)==GPIO_PIN_SET);
    }
}

void sil_man_001_005_006() {
    Fixture f;
    sil::set_spi_interframe_us(4); // component scenario; target CS-high readiness remains HIL-required
    sil::ads_model(0).set_calibration_duration(2000);
    CHECK(f.adc1.configureManualMode());
    CHECK(sil::ads_model(0).read_register(0) == 0x80);
    CHECK(sil::ads_model(0).read_register(5) == 0);
    CHECK((sil::ads_model(0).read_register(1) & 0xC0u) == 0);
    CHECK((sil::ads_model(0).read_register(1) & 0x08u) != 0);
    CHECK(sil::ads_model(0).read_register(0x10) == 0);
    for (uint8_t i=0; i<8; ++i) sil::set_sample(0, i, static_cast<uint32_t>(0x1200+i));
    CHECK(f.adc1.readAll());
    const uint32_t* values = &f.data1.AIN0;
    for (uint8_t i=0; i<8; ++i) CHECK(values[i] == static_cast<uint32_t>(0x1200+i));

    // The fake enforces the ADS mux pipeline: the first conversion after a
    // channel change still belongs to the old channel and must be discarded.
    Fixture mux;sil::set_spi_interframe_us(4);
    sil::set_sample(0,0,0x1111); sil::set_sample(0,3,0x3333);
    CHECK(mux.adc1.readChannel(3));
    CHECK(sil::transfers.size()>=2);
    const auto& discarded=sil::transfers[sil::transfers.size()-2].miso;
    const auto& selected=sil::transfers.back().miso;
    CHECK(discarded.size()==3 && discarded[0]==0x11 && discarded[1]==0x11);
    CHECK(selected.size()==3 && selected[0]==0x33 && selected[1]==0x33);
    CHECK(mux.data1.AIN3==0x3333);
}

void sil_man_002_003_004() {
    for (const unsigned clears_on : {1u, 5000u}) {
        Fixture f;
        sil::set_spi_interframe_us(4);
        sil::ads_model(0).set_calibration_duration(static_cast<uint64_t>(clears_on)*1000u);
        CHECK(f.adc1.configureManualMode());
        const auto status_writes = std::count_if(sil::transfers.begin(), sil::transfers.end(), [](const SilTransfer& t) {
            return t.mosi == std::vector<uint8_t>({0x08, 0x00, 0x00})
                || (t.mosi.size() == 3 && t.mosi[0] == 0x08 && t.mosi[1] == 0x00);
        });
        CHECK(status_writes == 1);
    }
    {
        Fixture f;
        sil::set_spi_interframe_us(4);sil::ads_model(0).set_calibration_duration(1000000000);
        CHECK(!f.adc1.configureManualMode());
        CHECK((sil::ads_model(0).read_register(1) & 0x02u) != 0u);
    }
    Fixture reference;sil::set_spi_interframe_us(4);
    CHECK(reference.adc1.configureManualMode());
    const std::size_t calls = sil::transfers.size();
    for (std::size_t fail_at = 0; fail_at < calls; ++fail_at) {
        Fixture f;
        sil::set_spi_interframe_us(4);
        for (std::size_t i = 0; i < fail_at; ++i) sil::script_status(HAL_OK);
        sil::script_status(HAL_ERROR);
        CHECK(!f.adc1.configureManualMode());
        CHECK(sil::transfers.size() == fail_at + 1);
        CHECK(HAL_GPIO_ReadPin(&sil::gpio1, 1) == GPIO_PIN_SET);
    }
}

void sil_man_006_failures() {
    for (uint8_t failed_channel = 0; failed_channel < 8; ++failed_channel) {
        Fixture f;
        sil::set_spi_interframe_us(4);
        for (uint8_t i = 0; i < 8; ++i) sil::set_sample(0, i, 0x2200u + i);
        for (unsigned i = 0; i < static_cast<unsigned>(failed_channel) * 5u; ++i) sil::script_status(HAL_OK);
        sil::script_status(HAL_ERROR);
        CHECK(!f.adc1.readAll());
        const uint32_t* values = &f.data1.AIN0;
        for (uint8_t i = 0; i < failed_channel; ++i) CHECK(values[i] == 0x2200u + i);
        for (uint8_t i = failed_channel; i < 8; ++i) CHECK(values[i] == 0u);
    }
}

void sil_auto_001_003() {
    bool all_req_order=true;
    for(const uint8_t mask:{uint8_t{0x01},uint8_t{0xA5},uint8_t{0xFF}}) {
        Fixture configured;
        CHECK(configured.adc1.configureAutonomousMode(mask));
        const std::array<std::vector<uint8_t>,6> expected{{
            {0x08,0x12,mask},{0x08,0x10,0x11},{0x08,0x04,0x0D},
            {0x08,0x03,0x03},{0x08,0x02,0x10},{0,0,0}}};
        CHECK(sil::transfers.size()==expected.size());
        for(std::size_t i=0;i<expected.size();++i) CHECK(sil::transfers[i].mosi==expected[i]);
        const auto sequence_index=std::find(expected.begin(),expected.end(),std::vector<uint8_t>{0x08,0x10,0x11})-expected.begin();
        const auto opmode_index=std::find(expected.begin(),expected.end(),std::vector<uint8_t>{0x08,0x04,0x0D})-expected.begin();
        const auto osr_index=std::find(expected.begin(),expected.end(),std::vector<uint8_t>{0x08,0x03,0x03})-expected.begin();
        const auto data_index=std::find(expected.begin(),expected.end(),std::vector<uint8_t>{0x08,0x02,0x10})-expected.begin();
        all_req_order &= opmode_index<sequence_index && osr_index<sequence_index && data_index<sequence_index;

        if(mask==0xA5) {
            const std::array<uint8_t,8> generated_order{{0,1,2,3,4,5,6,7}};
            for(const auto channel:generated_order) {
                // Isolate each valid tag from the known repeated-call MOSI
                // replay defect; one physical channel is enabled per fixture.
                Fixture physical;
                sil::set_sample(0,channel,0x3000u+channel);
                CHECK(physical.adc1.configureAutonomousMode(static_cast<uint8_t>(1u<<channel)));
                sil::advance_us(815);
                CHECK(physical.adc1.readChannelAuto());
                CHECK((&physical.data1.AIN0)[channel]==((0x3000u+channel)<<4u));
            }
        }
    }
    xfail_if(!all_req_order,"SIL-AUTO-001","DEF-AUTO-SEQ-START-ORDER");

}

void sil_adc_timing_characterization(){
    Fixture f; // default scripted CS-high overhead is deliberately zero
    sil::set_sample(0,0,0xBEEF);
    CHECK(f.adc1.readChannel(0));
    CHECK(f.data1.AIN0==0xBEEF);
    std::cout<<"BLOCKED SIL-ADC-TIMING target_CS_high_overhead_requires_oscilloscope_or_cycle_counter_evidence\n";
}

void sil_auto_002() {
    for (std::size_t fail_at = 0; fail_at < 6; ++fail_at) {
        Fixture f;
        for (std::size_t i = 0; i < fail_at; ++i) sil::script_status(HAL_OK);
        sil::script_status(HAL_TIMEOUT);
        CHECK(!f.adc1.configureAutonomousMode(0xFF));
        CHECK(sil::transfers.size() == fail_at + 1);
        CHECK(HAL_GPIO_ReadPin(&sil::gpio1, 1) == GPIO_PIN_SET);
    }
}

void sil_auto_004_005() {
    // OSR=8 is configured by the production driver, so a stable 16-bit
    // physical input must emerge as a 20-bit result shifted left by four.
    for(const uint32_t input:{0u,1u,0x7FFFu,0xFFFFu}){
        Fixture f;
        sil::set_sample(0,0,input);
        CHECK(f.adc1.configureAutonomousMode(0x01));
        sil::advance_us(815);
        CHECK(f.adc1.readChannelAuto());
        CHECK(f.data1.AIN0==(input<<4u));
    }

    // The model supplies only physical AIN0..AIN7. Invalid appended CHIDs are
    // injected at the serialized SDO boundary and must be rejected.
    for(uint8_t invalid=8;invalid<16;++invalid){
        Fixture f;
        sil::set_sample(0,0,0x4567u);
        CHECK(f.adc1.configureAutonomousMode(0x01));
        sil::advance_us(815);
        sil::script_next_miso_byte(2,0xF0u,invalid);
        CHECK(!f.adc1.readChannelAuto());
        CHECK(f.data1.AIN0==0u);
    }

    // Exercise the exact ready boundary and a deliberately delayed result.
    {
        Fixture early;
        sil::set_sample(0,0,0x2468u);
        CHECK(early.adc1.configureAutonomousMode(0x01));
        auto& model=sil::ads_model(0);
        CHECK(model.conversion_pending());
        const uint64_t before_ready=model.conversion_ready_ns()-model.now_ns()-1u;
        sil::ads_faults(0).advance_time(before_ready);
        const auto premature_before=model.premature_reads();
        CHECK(early.adc1.readChannelAuto());
        CHECK(model.premature_reads()==premature_before+1u);
        CHECK(early.data1.AIN0!=0x24680u);
    }
    {
        Fixture on_time;
        sil::set_sample(0,0,0x2468u);
        CHECK(on_time.adc1.configureAutonomousMode(0x01));
        auto& model=sil::ads_model(0);
        sil::ads_faults(0).advance_time(model.conversion_ready_ns()-model.now_ns());
        CHECK(on_time.adc1.readChannelAuto());
        CHECK(on_time.data1.AIN0==0x24680u);
    }
    {
        Fixture delayed;
        sil::set_sample(0,0,0x1357u);
        CHECK(delayed.adc1.configureAutonomousMode(0x01));
        auto& model=sil::ads_model(0);
        // Arm through the fault overlay. The first read completes the current
        // conversion and applies the delay to the next conversion on CS rising.
        sil::ads_faults(0).schedule({model.now_ns(),sil::ads7066::FaultKind::DelayReadiness,0,100000u});
        sil::ads_faults(0).advance_time(model.conversion_ready_ns()-model.now_ns());
        CHECK(delayed.adc1.readChannelAuto());
        CHECK(delayed.data1.AIN0==0x13570u);
        CHECK(model.conversion_pending());
        const uint64_t nominal_ready=model.conversion_ready_ns()-100000u;
        sil::ads_faults(0).advance_time(nominal_ready-model.now_ns());
        const auto premature_before=model.premature_reads();
        CHECK(delayed.adc1.readChannelAuto());
        CHECK(model.premature_reads()==premature_before+1u);
    }

    // Both production instances share SPI1 but retain independent analog state.
    {
        Fixture isolated;
        sil::set_sample(0,0,0x1111u);sil::set_sample(1,0,0xEEEEu);
        CHECK(isolated.adc1.configureAutonomousMode(0x01));
        CHECK(isolated.adc2.configureAutonomousMode(0x01));
        sil::advance_us(815);
        CHECK(isolated.adc1.readChannelAuto());
        CHECK(isolated.adc2.readChannelAuto());
        CHECK(isolated.data1.AIN0==0x11110u);
        CHECK(isolated.data2.AIN0==0xEEEE0u);
    }

    Fixture replay;
    sil::set_sample(0,0,0x3000u);
    CHECK(replay.adc1.configureAutonomousMode(0x01));
    sil::advance_us(815);CHECK(replay.adc1.readChannelAuto());
    const auto encoded=sil::transfers.back().miso;
    sil::advance_us(815);CHECK(replay.adc1.readChannelAuto());
    const bool replayed=sil::transfers.back().mosi==encoded;
    CHECK(replayed);
    xfail_if(replayed,"SIL-AUTO-005","DEF-AUTO-RESULT-MOSI-REPLAY");
    std::cout<<"BLOCKED SIL-AUTO-005 first_call_initializedness_requires_MemorySanitizer_or_equivalent\n";
}

void sil_fail_fast() {
    Fixture f;
    sil::script_status(HAL_ERROR);
    CHECK(!f.adc1.configureAutonomousMode(0xFF));
    CHECK(sil::transfers.size() == 1);
    CHECK(sil::transfers.front().mosi == std::vector<uint8_t>({0x08,0x12,0xFF}));
    sil::reset();
    sil::script_status(HAL_ERROR);
    CHECK(!f.adc1.configureManualMode());
    CHECK(sil::transfers.size() == 1);
    CHECK(sil::transfers.front().kind == SilTransfer::Kind::TransmitReceive);
}
}

int main() {
    const std::array<std::pair<const char*, std::function<void()>>,15> tests{{
        {"MODEL core", model_conformance_core},
        {"MODEL CRC",model_crc_conformance},
        {"SIL-SPI-001/002", sil_spi_001_002}, {"SIL-SPI-003 REQ/CHAR", sil_spi_003_req_char},
        {"SIL-ADC-001/002", sil_adc_001_002_isolation},
        {"SIL-ADC-003/004", sil_adc_003_004}, {"SIL-MAN-001/005/006", sil_man_001_005_006},
        {"SIL-ADC-TIMING",sil_adc_timing_characterization},
        {"SIL-MAN-002/003/004", sil_man_002_003_004},
        {"SIL-MAN-006 failures", sil_man_006_failures},
        {"SIL-AUTO-001/003", sil_auto_001_003}, {"SIL-AUTO-002", sil_auto_002},
        {"SIL-AUTO-004/005",sil_auto_004_005},
        {"HAL wrapper coverage",sil_hal_wrapper_coverage},
        {"failure propagation", sil_fail_fast}}};
    for (const auto& [name, test] : tests) {
        const int prior = failures;
        try { test(); } catch (const std::exception& error) { std::cerr << name << ": " << error.what() << '\n'; ++failures; }
        std::cout << (failures == prior ? "PASS " : "FAIL ") << name << '\n';
    }
    std::cout << "KNOWN_DEFECTS " << known_defects << '\n';
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
