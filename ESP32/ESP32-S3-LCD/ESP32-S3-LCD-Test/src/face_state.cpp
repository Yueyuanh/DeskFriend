#include "face_state.h"

FaceRequestState faceRequest;

void FaceRequestState::update(const FaceUpdate& update) {
  const uint32_t now = millis();
  portENTER_CRITICAL(&mux_);
  value_ = update;
  value_.seq = ++seq_;
  value_.updatedMs = now;
  portEXIT_CRITICAL(&mux_);
}

void FaceRequestState::displayed(uint32_t seq, uint32_t renderUs, uint32_t flushUs) {
  portENTER_CRITICAL(&mux_);
  displayedSeq_ = seq; renderUs_ = renderUs; flushUs_ = flushUs;
  portEXIT_CRITICAL(&mux_);
}
void FaceRequestState::metrics(uint32_t& seq, uint32_t& renderUs, uint32_t& flushUs) {
  portENTER_CRITICAL(&mux_);
  seq = displayedSeq_; renderUs = renderUs_; flushUs = flushUs_;
  portEXIT_CRITICAL(&mux_);
}

FaceUpdate FaceRequestState::snapshot() {
  portENTER_CRITICAL(&mux_);
  FaceUpdate copy = value_;
  portEXIT_CRITICAL(&mux_);
  return copy;
}
