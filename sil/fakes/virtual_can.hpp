#pragma once

#include <array>
#include <cstdint>
#include <deque>
#include <vector>

namespace sil::vcan {

struct Frame {
    uint32_t identifier{};
    uint8_t length{};
    std::array<uint8_t, 8> data{};
    bool extended{};
    bool remote{};
    bool fd{};
    bool bit_rate_switch{};
};

enum class NodeMode { Active, ListenOnly, Offline };
enum class ProtocolError { None, Stuff, Form, Ack, Bit1, Bit0, Crc };
enum class ControllerEventKind { Acknowledged, AttemptError };

struct Attempt {
    uint64_t time_us{};
    unsigned token{};
    unsigned number{};
    Frame frame{};
};

struct ControllerEvent {
    uint64_t time_us{};
    ControllerEventKind kind{};
    unsigned token{};
    unsigned attempt{};
    ProtocolError error{};
};

class VirtualCanNode {
public:
    void reset();
    void set_mode(NodeMode mode) { mode_ = mode; }
    NodeMode mode() const { return mode_; }
    void set_frame_completion_latency_us(uint64_t delay) { completion_latency_us_ = delay; }
    uint64_t frame_completion_latency_us() const { return completion_latency_us_; }
    void script_error(ProtocolError error, unsigned attempts = 1, uint64_t delay_us = 0);

private:
    friend class VirtualCanBus;
    struct ErrorScript { ProtocolError error; unsigned remaining; uint64_t delay_us; };
    NodeMode mode_{NodeMode::Active};
    uint64_t completion_latency_us_{};
    std::deque<ErrorScript> errors_;
};

class VirtualCanBus {
public:
    explicit VirtualCanBus(VirtualCanNode& node) : node_(node) {}
    void reset();
    bool queue_from_dut(unsigned token, const Frame& frame, unsigned max_attempts = 1,
                        uint64_t retry_delay_us = 1000);
    bool retire_request(unsigned token);
    void advance_by(uint64_t delta_us);
    void advance_to(uint64_t time_us);
    uint64_t now_us() const { return now_us_; }

    const std::vector<Frame>& queued_frames() const { return queued_; }
    const std::vector<Attempt>& attempted_frames() const { return attempted_; }
    const std::vector<Frame>& received_frames() const { return received_; }
    const std::vector<ControllerEvent>& event_log() const { return events_; }
    std::vector<ControllerEvent> take_controller_events();

private:
    struct Request {
        unsigned token{};
        Frame frame{};
        unsigned attempt{};
        unsigned max_attempts{1};
        uint64_t retry_delay_us{1000};
        bool active{};
    };
    struct Scheduled {
        enum class Kind { BeginAttempt, ResolveAttempt } kind{Kind::BeginAttempt};
        uint64_t due_us{};
        unsigned token{};
        unsigned attempt{};
        ProtocolError outcome{};
        bool peer_received{};
    };

    VirtualCanNode& node_;
    uint64_t now_us_{};
    std::vector<Request> requests_;
    std::vector<Scheduled> scheduled_;
    std::vector<Frame> queued_;
    std::vector<Attempt> attempted_;
    std::vector<Frame> received_;
    std::vector<ControllerEvent> events_;
    std::vector<ControllerEvent> pending_events_;

    Request* find_request(unsigned token);
    void begin_attempt(Request& request, uint64_t at_us);
    void resolve(const Scheduled& scheduled);
};

} // namespace sil::vcan
