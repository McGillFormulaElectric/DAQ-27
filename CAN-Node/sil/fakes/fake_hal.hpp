#pragma once

#include "../ads7066_fault_overlay.hpp"
#include "virtual_can.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <string>
#include <vector>

struct SPI_HandleTypeDef { int instance; };
struct GPIO_TypeDef { int instance; };
struct FDCAN_GlobalTypeDef { uint32_t TXBRP{}; uint32_t TXBTO{}; };
struct FDCAN_HandleTypeDef { FDCAN_GlobalTypeDef* Instance; };
struct FDCAN_FilterTypeDef { uint32_t IdType{}, FilterIndex{}, FilterType{}, FilterConfig{}, FilterID1{}, FilterID2{}; };
struct FDCAN_TxHeaderTypeDef { uint32_t Identifier{}, IdType{}, TxFrameType{}, DataLength{}, ErrorStateIndicator{}, BitRateSwitch{}, FDFormat{}, TxEventFifoControl{}, MessageMarker{}; };
struct FDCAN_ProtocolStatusTypeDef { uint32_t LastErrorCode{}, BusOff{}, ErrorPassive{}; };
struct FDCAN_ErrorCountersTypeDef { uint32_t TxErrorCnt{}, RxErrorCnt{}; };

enum HAL_StatusTypeDef { HAL_OK = 0, HAL_ERROR = 1, HAL_BUSY = 2, HAL_TIMEOUT = 3 };
enum GPIO_PinState { GPIO_PIN_RESET = 0, GPIO_PIN_SET = 1 };

struct SilTransfer {
    enum class Kind { Transmit, Receive, TransmitReceive } kind;
    int device;
    std::vector<uint8_t> mosi;
    std::vector<uint8_t> miso;
    uint32_t timeout;
};

struct SilAdsEndpoint {
    sil::ads7066::Ads7066Model model;
    sil::ads7066::Ads7066FaultOverlay faults;
    int reset_after_transfers{-1};
    unsigned spontaneous_resets{0};
    SilAdsEndpoint();
    SilAdsEndpoint(const SilAdsEndpoint&)=delete;
    SilAdsEndpoint& operator=(const SilAdsEndpoint&)=delete;
};

struct SilCanFrame { FDCAN_TxHeaderTypeDef header; std::array<uint8_t,8> data{}; };

namespace sil {
extern GPIO_TypeDef gpio1;
extern GPIO_TypeDef gpio2;
extern std::array<std::unique_ptr<SilAdsEndpoint>, 2> ads;
extern std::vector<SilTransfer> transfers;
extern std::vector<std::pair<int, GPIO_PinState>> gpio_events;
extern std::vector<SilCanFrame> can_frames;
extern std::vector<uint32_t> delays;
extern std::vector<std::string> can_calls;
extern FDCAN_FilterTypeDef last_filter;
extern FDCAN_ProtocolStatusTypeDef protocol_status;
extern FDCAN_ErrorCountersTypeDef error_counters;
extern vcan::VirtualCanNode can_peer;
extern vcan::VirtualCanBus can_bus;
extern uint64_t virtual_us;

void reset();
void script_status(HAL_StatusTypeDef status);
void script_next_miso_byte(std::size_t byte, uint8_t and_mask, uint8_t or_mask);
void set_sample(int device, uint8_t channel, uint32_t value);
void queue_auto_result(int device, uint8_t channel, uint32_t value);
void queue_auto_result_at(int device, uint8_t channel, uint32_t value, uint64_t ready_us);
void advance_us(uint64_t amount);
void set_spi_interframe_us(uint64_t amount);
void set_spi_bit_cell_ns(uint64_t amount);
void schedule_ads_reset_after_transfers(int device, unsigned transfers);
bool all_chip_selects_high();
uint8_t crc8_ccitt(const uint8_t* data, std::size_t size);
sil::ads7066::Ads7066Model& ads_model(int device);
sil::ads7066::Ads7066FaultOverlay& ads_faults(int device);
void script_can_status(HAL_StatusTypeDef status);
void set_can_peer_mode(vcan::NodeMode mode);
void set_can_frame_completion_latency_us(uint64_t delay_us);
void script_can_protocol_error(vcan::ProtocolError error, unsigned attempts=1, uint64_t delay_us=0);
void set_can_retry_policy(unsigned max_attempts, uint64_t retry_delay_us);
}

extern "C" {
HAL_StatusTypeDef HAL_SPI_Transmit(SPI_HandleTypeDef*, uint8_t*, uint16_t, uint32_t);
HAL_StatusTypeDef HAL_SPI_Receive(SPI_HandleTypeDef*, uint8_t*, uint16_t, uint32_t);
HAL_StatusTypeDef HAL_SPI_TransmitReceive(SPI_HandleTypeDef*, uint8_t*, uint8_t*, uint16_t, uint32_t);
void HAL_GPIO_WritePin(GPIO_TypeDef*, uint16_t, GPIO_PinState);
GPIO_PinState HAL_GPIO_ReadPin(GPIO_TypeDef*, uint16_t);
void HAL_GPIO_TogglePin(GPIO_TypeDef*, uint16_t);
void HAL_Delay(uint32_t);
HAL_StatusTypeDef HAL_FDCAN_ConfigFilter(FDCAN_HandleTypeDef*, FDCAN_FilterTypeDef*);
HAL_StatusTypeDef HAL_FDCAN_ConfigGlobalFilter(FDCAN_HandleTypeDef*, uint32_t, uint32_t, uint32_t, uint32_t);
HAL_StatusTypeDef HAL_FDCAN_Start(FDCAN_HandleTypeDef*);
HAL_StatusTypeDef HAL_FDCAN_Stop(FDCAN_HandleTypeDef*);
HAL_StatusTypeDef HAL_FDCAN_ActivateNotification(FDCAN_HandleTypeDef*, uint32_t, uint32_t);
HAL_StatusTypeDef HAL_FDCAN_AddMessageToTxFifoQ(FDCAN_HandleTypeDef*, FDCAN_TxHeaderTypeDef*, uint8_t*);
HAL_StatusTypeDef HAL_FDCAN_GetProtocolStatus(FDCAN_HandleTypeDef*, FDCAN_ProtocolStatusTypeDef*);
HAL_StatusTypeDef HAL_FDCAN_GetErrorCounters(FDCAN_HandleTypeDef*, FDCAN_ErrorCountersTypeDef*);
}
