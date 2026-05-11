//
// Implementation of wrtc::E2EFrameTransformer.
//
// Threading: libwebrtc may call Transform() on its worker / encoder /
// decoder threads. The user-supplied callback must be thread-safe; we
// hold mutex_ only for sink-callback registry lookups, NOT during the
// callback invocation (so the callback can take long without blocking
// other SSRCs).
//

#include <wrtc/interfaces/media/e2e_frame_transformer.hpp>

#include <utility>

namespace wrtc {

E2EFrameTransformer::E2EFrameTransformer(int64_t user_id,
                                          bool is_outgoing,
                                          E2EFrameCallback callback)
    : default_user_id_(user_id),
      is_outgoing_(is_outgoing),
      callback_(std::move(callback)) {}

void E2EFrameTransformer::Transform(
    std::unique_ptr<webrtc::TransformableFrameInterface> frame) {
    if (!callback_) {
        // No callback configured — pass through (should not happen if
        // the transformer is registered, but be defensive).
        webrtc::scoped_refptr<webrtc::TransformedFrameCallback> sink;
        {
            webrtc::MutexLock lock(&mutex_);
            const uint32_t ssrc = frame->GetSsrc();
            auto it = ssrc_sinks_.find(ssrc);
            sink = (it != ssrc_sinks_.end()) ? it->second : default_sink_;
        }
        if (sink) sink->OnTransformedFrame(std::move(frame));
        return;
    }

    // Snapshot frame bytes and metadata.
    auto data_view = frame->GetData();
    std::vector<uint8_t> input(data_view.begin(), data_view.end());
    const uint32_t ssrc = frame->GetSsrc();

    int64_t user_id = default_user_id_;
    webrtc::scoped_refptr<webrtc::TransformedFrameCallback> sink;
    {
        webrtc::MutexLock lock(&mutex_);
        if (!is_outgoing_) {
            auto uit = ssrc_user_ids_.find(ssrc);
            if (uit != ssrc_user_ids_.end()) user_id = uit->second;
        }
        auto it = ssrc_sinks_.find(ssrc);
        sink = (it != ssrc_sinks_.end()) ? it->second : default_sink_;
    }
    if (!sink) {
        // Nowhere to deliver — drop.
        return;
    }

    // Audio frames have no plaintext header; video frames have a small
    // unencrypted prefix. libwebrtc exposes this via the video-specific
    // subtype only — for the audio path we always pass 0. Phase 2 scope
    // is audio-only; video support would derive header size from
    // TransformableVideoFrameInterface::GetMetadata().
    constexpr int32_t kPlaintextHeaderSize = 0;

    std::vector<uint8_t> output = callback_(
        input, user_id, is_outgoing_, kPlaintextHeaderSize);

    if (output.empty()) {
        // Silent drop, matching tgcalls semantics.
        return;
    }

    frame->SetData(output);
    sink->OnTransformedFrame(std::move(frame));
}

void E2EFrameTransformer::RegisterTransformedFrameCallback(
    webrtc::scoped_refptr<webrtc::TransformedFrameCallback> callback) {
    webrtc::MutexLock lock(&mutex_);
    default_sink_ = std::move(callback);
}

void E2EFrameTransformer::UnregisterTransformedFrameCallback() {
    webrtc::MutexLock lock(&mutex_);
    default_sink_ = nullptr;
}

void E2EFrameTransformer::RegisterTransformedFrameSinkCallback(
    webrtc::scoped_refptr<webrtc::TransformedFrameCallback> callback,
    uint32_t ssrc) {
    webrtc::MutexLock lock(&mutex_);
    ssrc_sinks_[ssrc] = std::move(callback);
}

void E2EFrameTransformer::UnregisterTransformedFrameSinkCallback(uint32_t ssrc) {
    webrtc::MutexLock lock(&mutex_);
    ssrc_sinks_.erase(ssrc);
    ssrc_user_ids_.erase(ssrc);
}

void E2EFrameTransformer::SetSenderUserId(uint32_t ssrc, int64_t user_id) {
    webrtc::MutexLock lock(&mutex_);
    ssrc_user_ids_[ssrc] = user_id;
}

}  // namespace wrtc
