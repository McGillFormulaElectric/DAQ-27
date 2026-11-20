#include "fake_hal.hpp"

#include <algorithm>
#include <stdexcept>

SPI_HandleTypeDef hspi1{1};
SPI_HandleTypeDef hspi3{3};
static FDCAN_GlobalTypeDef fdcan_regs;
FDCAN_HandleTypeDef hfdcan1{&fdcan_regs};

SilAdsEndpoint::SilAdsEndpoint():faults(model){model.set_calibration_duration(1);model.advance_time(5000000);}

namespace sil {
GPIO_TypeDef gpio1{1};
GPIO_TypeDef gpio2{2};
std::array<std::unique_ptr<SilAdsEndpoint>,2> ads{{std::make_unique<SilAdsEndpoint>(),std::make_unique<SilAdsEndpoint>()}};
std::vector<SilTransfer> transfers;
std::vector<std::pair<int,GPIO_PinState>> gpio_events;
std::vector<SilCanFrame> can_frames;
std::vector<uint32_t> delays;
std::vector<std::string> can_calls;
FDCAN_FilterTypeDef last_filter{};
FDCAN_ProtocolStatusTypeDef protocol_status{};
FDCAN_ErrorCountersTypeDef error_counters{};
vcan::VirtualCanNode can_peer;
vcan::VirtualCanBus can_bus{can_peer};
uint64_t virtual_us{};
static std::array<GPIO_PinState,2> cs{GPIO_PIN_SET,GPIO_PIN_SET};
static std::deque<HAL_StatusTypeDef> statuses;
static std::deque<HAL_StatusTypeDef> can_statuses;
struct MisoBytePatch { std::size_t byte; uint8_t and_mask; uint8_t or_mask; };
static std::deque<MisoBytePatch> miso_patches;
static uint64_t scripted_interframe_us{};
// Conditional scenario value. Production SPI1 kernel-clock authority is not yet archived.
static uint64_t scripted_bit_cell_ns{200u};
static std::array<bool,3> can_occupied{};
static unsigned can_put_index{};
static unsigned can_max_attempts{1024};
static uint64_t can_retry_delay_us{1000};
static std::size_t synced_received{};

static uint32_t can_lec(vcan::ProtocolError error){switch(error){case vcan::ProtocolError::Stuff:return 1;case vcan::ProtocolError::Form:return 2;case vcan::ProtocolError::Ack:return 3;case vcan::ProtocolError::Bit1:return 4;case vcan::ProtocolError::Bit0:return 5;case vcan::ProtocolError::Crc:return 6;default:return 0;}}
static void apply_can_events(){
    for(const auto& event:can_bus.take_controller_events()){
        if(event.token>=can_occupied.size())continue;
        const uint32_t bit=1u<<event.token;
        if(event.kind==vcan::ControllerEventKind::Acknowledged){can_occupied[event.token]=false;fdcan_regs.TXBRP&=~bit;fdcan_regs.TXBTO|=bit;protocol_status.LastErrorCode=0;}
        else {protocol_status.LastErrorCode=can_lec(event.error);}
    }
    const auto& received=can_bus.received_frames();
    while(synced_received<received.size()){
        const auto& source=received[synced_received++];SilCanFrame frame{};frame.header.Identifier=source.identifier;frame.header.DataLength=source.length;frame.data=source.data;can_frames.push_back(frame);
    }
}

ads7066::Ads7066Model& ads_model(int device){return ads.at(static_cast<std::size_t>(device))->model;}
ads7066::Ads7066FaultOverlay& ads_faults(int device){return ads.at(static_cast<std::size_t>(device))->faults;}

void reset(){
    transfers.clear();gpio_events.clear();statuses.clear();can_statuses.clear();miso_patches.clear();can_frames.clear();delays.clear();can_calls.clear();
    last_filter={};protocol_status={};error_counters={};virtual_us=0;scripted_interframe_us=0;scripted_bit_cell_ns=200u;fdcan_regs={};cs={GPIO_PIN_SET,GPIO_PIN_SET};
    can_occupied={};can_put_index=0;can_max_attempts=1024;can_retry_delay_us=1000;synced_received=0;can_bus.reset();
    for(auto& endpoint:ads)endpoint=std::make_unique<SilAdsEndpoint>();
}
void script_status(HAL_StatusTypeDef status){statuses.push_back(status);}
void script_next_miso_byte(std::size_t byte,uint8_t and_mask,uint8_t or_mask){miso_patches.push_back({byte,and_mask,or_mask});}
void set_sample(int device,uint8_t channel,uint32_t value){ads_model(device).set_analog_code(channel,value);}
void queue_auto_result(int device,uint8_t channel,uint32_t value){queue_auto_result_at(device,channel,value,virtual_us);}
void queue_auto_result_at(int device,uint8_t channel,uint32_t value,uint64_t ready_us){ads_model(device).queue_analog_code(channel,value,ads_model(device).now_ns()+(ready_us>virtual_us?(ready_us-virtual_us)*1000u:0u));}
void advance_us(uint64_t amount){virtual_us+=amount;for(auto& endpoint:ads)endpoint->faults.advance_time(amount*1000u);can_bus.advance_by(amount);apply_can_events();}
void set_spi_interframe_us(uint64_t amount){scripted_interframe_us=amount;}
void set_spi_bit_cell_ns(uint64_t amount){if(amount<20u)throw std::invalid_argument("SPI bit cell shorter than model edge resolution");scripted_bit_cell_ns=amount;}
void schedule_ads_reset_after_transfers(int device,unsigned count){ads.at(static_cast<std::size_t>(device))->reset_after_transfers=static_cast<int>(count);}
bool all_chip_selects_high(){return cs[0]==GPIO_PIN_SET&&cs[1]==GPIO_PIN_SET;}
uint8_t crc8_ccitt(const uint8_t* data,std::size_t size){return ads7066::Ads7066Model::crc8(data,size);}
void script_can_status(HAL_StatusTypeDef status){can_statuses.push_back(status);}
void set_can_peer_mode(vcan::NodeMode mode){can_peer.set_mode(mode);}
void set_can_frame_completion_latency_us(uint64_t delay_us){can_peer.set_frame_completion_latency_us(delay_us);}
void script_can_protocol_error(vcan::ProtocolError error,unsigned attempts,uint64_t delay_us){can_peer.script_error(error,attempts,delay_us);}
void set_can_retry_policy(unsigned attempts,uint64_t delay_us){can_max_attempts=attempts?attempts:1;can_retry_delay_us=delay_us;}

static HAL_StatusTypeDef next_status(){if(statuses.empty())return HAL_OK;const auto value=statuses.front();statuses.pop_front();return value;}
static HAL_StatusTypeDef next_can_status(){if(can_statuses.empty())return HAL_OK;const auto value=can_statuses.front();can_statuses.pop_front();return value;}
static int selected(){if(cs[0]==GPIO_PIN_RESET&&cs[1]==GPIO_PIN_SET)return 0;if(cs[1]==GPIO_PIN_RESET&&cs[0]==GPIO_PIN_SET)return 1;return -1;}
static std::vector<uint8_t> bytes_or_zero(const uint8_t* data,uint16_t size){std::vector<uint8_t> bytes(size);if(data)std::copy(data,data+size,bytes.begin());return bytes;}

static void transact(SilTransfer::Kind kind,uint8_t* tx,uint8_t* rx,uint16_t size,uint32_t timeout){
    const int device=selected();if(device<0)throw std::runtime_error("SPI transfer without exactly one selected ADC");
    auto& endpoint=*ads[static_cast<std::size_t>(device)];
    if(endpoint.reset_after_transfers==0){endpoint.model.brownout();endpoint.reset_after_transfers=-1;++endpoint.spontaneous_resets;}
    else if(endpoint.reset_after_transfers>0)--endpoint.reset_after_transfers;
    std::vector<uint8_t> mosi=bytes_or_zero(tx,size),miso(size);
    for(std::size_t byte=0;byte<size;++byte)for(int bit=7;bit>=0;--bit){
        if(endpoint.faults.clock_spi_bit((mosi[byte]&(1u<<bit))!=0u)==ads7066::PinLevel::High)miso[byte]|=static_cast<uint8_t>(1u<<bit);
        endpoint.model.advance_time(scripted_bit_cell_ns-20u); // clock_spi_bit accounts for the first 20 ns.
        ads[static_cast<std::size_t>(1-device)]->model.advance_time(scripted_bit_cell_ns);
    }
    if(!miso_patches.empty()){
        const auto patch=miso_patches.front();miso_patches.pop_front();
        if(patch.byte>=miso.size())throw std::out_of_range("scripted MISO byte is outside transfer");
        miso[patch.byte]=static_cast<uint8_t>((miso[patch.byte]&patch.and_mask)|patch.or_mask);
    }
    if(rx)std::copy(miso.begin(),miso.end(),rx);
    transfers.push_back({kind,device,std::move(mosi),std::move(miso),timeout});
}
}

extern "C" HAL_StatusTypeDef HAL_SPI_Transmit(SPI_HandleTypeDef*,uint8_t* data,uint16_t size,uint32_t timeout){const auto status=sil::next_status();if(status==HAL_OK)sil::transact(SilTransfer::Kind::Transmit,data,nullptr,size,timeout);else sil::transfers.push_back({SilTransfer::Kind::Transmit,sil::selected(),sil::bytes_or_zero(data,size),{},timeout});return status;}
extern "C" HAL_StatusTypeDef HAL_SPI_Receive(SPI_HandleTypeDef*,uint8_t* data,uint16_t size,uint32_t timeout){const auto status=sil::next_status();if(status==HAL_OK)sil::transact(SilTransfer::Kind::Receive,data,data,size,timeout);else sil::transfers.push_back({SilTransfer::Kind::Receive,sil::selected(),sil::bytes_or_zero(data,size),{},timeout});return status;}
extern "C" HAL_StatusTypeDef HAL_SPI_TransmitReceive(SPI_HandleTypeDef*,uint8_t* tx,uint8_t* rx,uint16_t size,uint32_t timeout){const auto status=sil::next_status();if(status==HAL_OK)sil::transact(SilTransfer::Kind::TransmitReceive,tx,rx,size,timeout);else sil::transfers.push_back({SilTransfer::Kind::TransmitReceive,sil::selected(),sil::bytes_or_zero(tx,size),{},timeout});return status;}
extern "C" void HAL_GPIO_WritePin(GPIO_TypeDef* port,uint16_t,GPIO_PinState state){const int index=port==&sil::gpio1?0:(port==&sil::gpio2?1:-1);if(index>=0){sil::cs[static_cast<std::size_t>(index)]=state;sil::gpio_events.emplace_back(index,state);sil::ads_faults(index).set_cs(state==GPIO_PIN_SET);if(state==GPIO_PIN_SET&&sil::scripted_interframe_us!=0)sil::advance_us(sil::scripted_interframe_us);}}
extern "C" GPIO_PinState HAL_GPIO_ReadPin(GPIO_TypeDef* port,uint16_t){if(port==&sil::gpio1)return sil::cs[0];if(port==&sil::gpio2)return sil::cs[1];throw std::runtime_error("HAL_GPIO_ReadPin called with unknown SIL port");}
extern "C" void HAL_GPIO_TogglePin(GPIO_TypeDef* port,uint16_t pin){HAL_GPIO_WritePin(port,pin,HAL_GPIO_ReadPin(port,pin)==GPIO_PIN_SET?GPIO_PIN_RESET:GPIO_PIN_SET);}
extern "C" void HAL_Delay(uint32_t ms){sil::delays.push_back(ms);sil::advance_us(static_cast<uint64_t>(ms)*1000u);}
extern "C" HAL_StatusTypeDef HAL_FDCAN_ConfigFilter(FDCAN_HandleTypeDef*,FDCAN_FilterTypeDef* f){sil::can_calls.push_back("filter");sil::last_filter=*f;return sil::next_can_status();}
extern "C" HAL_StatusTypeDef HAL_FDCAN_ConfigGlobalFilter(FDCAN_HandleTypeDef*,uint32_t,uint32_t,uint32_t,uint32_t){sil::can_calls.push_back("global");return sil::next_can_status();}
extern "C" HAL_StatusTypeDef HAL_FDCAN_Start(FDCAN_HandleTypeDef*){sil::can_calls.push_back("start");return sil::next_can_status();}
extern "C" HAL_StatusTypeDef HAL_FDCAN_Stop(FDCAN_HandleTypeDef*){sil::can_calls.push_back("stop");return sil::next_can_status();}
extern "C" HAL_StatusTypeDef HAL_FDCAN_ActivateNotification(FDCAN_HandleTypeDef*,uint32_t,uint32_t){sil::can_calls.push_back("notify");return sil::next_can_status();}
extern "C" HAL_StatusTypeDef HAL_FDCAN_AddMessageToTxFifoQ(FDCAN_HandleTypeDef*,FDCAN_TxHeaderTypeDef* header,uint8_t* data){
    const auto status=sil::next_can_status();sil::can_calls.push_back("tx");if(status!=HAL_OK)return status;
    int selected=-1;for(unsigned offset=0;offset<sil::can_occupied.size();++offset){const unsigned candidate=(sil::can_put_index+offset)%sil::can_occupied.size();if(!sil::can_occupied[candidate]){selected=static_cast<int>(candidate);break;}}
    if(selected<0)return HAL_BUSY;
    const unsigned buffer=static_cast<unsigned>(selected);const uint32_t bit=1u<<buffer;
    sil::can_occupied[buffer]=true;sil::can_put_index=(buffer+1)%sil::can_occupied.size();fdcan_regs.TXBTO&=~bit;fdcan_regs.TXBRP|=bit;
    sil::vcan::Frame frame{};frame.identifier=header->Identifier;frame.length=static_cast<uint8_t>(std::min<uint32_t>(header->DataLength,8));std::copy(data,data+frame.length,frame.data.begin());
    if(!sil::can_bus.queue_from_dut(buffer,frame,sil::can_max_attempts,sil::can_retry_delay_us)){sil::can_occupied[buffer]=false;fdcan_regs.TXBRP&=~bit;return HAL_ERROR;}
    sil::apply_can_events();return HAL_OK;
}
extern "C" HAL_StatusTypeDef HAL_FDCAN_GetProtocolStatus(FDCAN_HandleTypeDef*,FDCAN_ProtocolStatusTypeDef* out){sil::can_calls.push_back("psr");*out=sil::protocol_status;return sil::next_can_status();}
extern "C" HAL_StatusTypeDef HAL_FDCAN_GetErrorCounters(FDCAN_HandleTypeDef*,FDCAN_ErrorCountersTypeDef* out){sil::can_calls.push_back("ecr");*out=sil::error_counters;return sil::next_can_status();}
