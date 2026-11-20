#include "virtual_can.hpp"

#include <algorithm>

namespace sil::vcan {

void VirtualCanNode::reset() {
    mode_ = NodeMode::Active;
    completion_latency_us_ = 0;
    errors_.clear();
}

void VirtualCanNode::script_error(ProtocolError error, unsigned attempts, uint64_t delay_us) {
    if (error != ProtocolError::None && attempts != 0) errors_.push_back({error, attempts, delay_us});
}

void VirtualCanBus::reset() {
    now_us_ = 0;
    std::vector<Request>().swap(requests_);
    std::vector<Scheduled>().swap(scheduled_);
    std::vector<Frame>().swap(queued_);
    std::vector<Attempt>().swap(attempted_);
    std::vector<Frame>().swap(received_);
    std::vector<ControllerEvent>().swap(events_);
    std::vector<ControllerEvent>().swap(pending_events_);
    node_.reset();
}

VirtualCanBus::Request* VirtualCanBus::find_request(unsigned token) {
    const auto it = std::find_if(requests_.begin(), requests_.end(),
        [token](const Request& request) { return request.token == token && request.active; });
    return it == requests_.end() ? nullptr : &*it;
}

bool VirtualCanBus::queue_from_dut(unsigned token, const Frame& frame, unsigned max_attempts,
                                   uint64_t retry_delay_us) {
    if (find_request(token) || frame.identifier > (frame.extended ? 0x1FFFFFFFu : 0x7FFu) ||
        frame.length > 8 || frame.fd || frame.remote) return false;
    queued_.push_back(frame);
    requests_.push_back({token, frame, 0, std::max(1u, max_attempts), retry_delay_us, true});
    begin_attempt(requests_.back(), now_us_);
    advance_to(now_us_); // resolve immediate ACK/error deterministically
    return true;
}

bool VirtualCanBus::retire_request(unsigned token) {
    auto* request=find_request(token);if(!request)return false;request->active=false;
    scheduled_.erase(std::remove_if(scheduled_.begin(),scheduled_.end(),[token](const Scheduled& item){return item.token==token;}),scheduled_.end());
    return true;
}

void VirtualCanBus::begin_attempt(Request& request, uint64_t at_us) {
    ++request.attempt;
    attempted_.push_back({at_us, request.token, request.attempt, request.frame});

    ProtocolError outcome = ProtocolError::None;
    uint64_t delay = node_.completion_latency_us_;
    if (!node_.errors_.empty()) {
        auto& script = node_.errors_.front();
        outcome = script.error;
        delay = script.delay_us;
        if (--script.remaining == 0) node_.errors_.pop_front();
    } else if (node_.mode_ != NodeMode::Active) {
        outcome = ProtocolError::Ack;
    }
    const bool received = node_.mode_ != NodeMode::Offline &&
                          (outcome == ProtocolError::None || outcome == ProtocolError::Ack);
    scheduled_.push_back({Scheduled::Kind::ResolveAttempt,at_us + delay, request.token, request.attempt, outcome, received});
}

void VirtualCanBus::resolve(const Scheduled& scheduled) {
    auto* request = find_request(scheduled.token);
    if (!request) return;
    if(scheduled.kind==Scheduled::Kind::BeginAttempt) { begin_attempt(*request,now_us_); return; }
    if (request->attempt != scheduled.attempt) return;
    if (scheduled.peer_received) received_.push_back(request->frame);
    if (scheduled.outcome == ProtocolError::None) {
        request->active = false;
        ControllerEvent event{now_us_, ControllerEventKind::Acknowledged, scheduled.token,
                              scheduled.attempt, ProtocolError::None};
        events_.push_back(event); pending_events_.push_back(event);
        return;
    }

    ControllerEvent error{now_us_, ControllerEventKind::AttemptError, scheduled.token,
                          scheduled.attempt, scheduled.outcome};
    events_.push_back(error); pending_events_.push_back(error);
    if (request->attempt < request->max_attempts) {
        scheduled_.push_back({Scheduled::Kind::BeginAttempt,now_us_ + request->retry_delay_us,
                              request->token,request->attempt,ProtocolError::None,false});
    }
}

void VirtualCanBus::advance_by(uint64_t delta_us) { advance_to(now_us_ + delta_us); }

void VirtualCanBus::advance_to(uint64_t time_us) {
    if (time_us < now_us_) return;
    for (;;) {
        const auto it = std::min_element(scheduled_.begin(), scheduled_.end(),
            [](const Scheduled& left, const Scheduled& right) { return left.due_us < right.due_us; });
        if (it == scheduled_.end() || it->due_us > time_us) break;
        const Scheduled next = *it;
        scheduled_.erase(it);
        now_us_ = next.due_us;
        resolve(next);
    }
    now_us_ = time_us;
}

std::vector<ControllerEvent> VirtualCanBus::take_controller_events() {
    auto result = std::move(pending_events_);
    pending_events_.clear();
    return result;
}

} // namespace sil::vcan
