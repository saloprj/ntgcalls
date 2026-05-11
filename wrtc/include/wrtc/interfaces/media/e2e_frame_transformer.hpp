//
// E2E frame transformer — surfaces libwebrtc's FrameTransformerInterface
// to a user-supplied callback so per-frame encrypt/decrypt can run in
// userland (e.g. TDLib's tde2e_api::call_encrypt / call_decrypt).
//
// Wire identical to tgcalls' GroupInstanceDescriptor.e2eEncryptDecrypt
// (signature: buffer, user_id, is_outgoing, plaintext_header_size) so
// downstream consumers can mirror what desktop / iOS / Android do.
//

#pragma once

#include <api/frame_transformer_interface.h>
#include <api/scoped_refptr.h>
#include <rtc_base/synchronization/mutex.h>

#include <cstdint>
#include <functional>
#include <map>
#include <vector>

namespace wrtc {

// Callback signature mirrors tgcalls' e2eEncryptDecrypt:
//   in: frame buffer, user_id (sender or self), is_outgoing (true=encrypt /
//       false=decrypt), plaintext_header_size (video only; 0 for audio)
//   out: transformed buffer; EMPTY = silently drop this frame
using E2EFrameCallback = std::function<std::vector<uint8_t>(
    const std::vector<uint8_t>&, int64_t, bool, int32_t)>;

class E2EFrameTransformer : public webrtc::FrameTransformerInterface {
public:
    // user_id: the local user_id when is_outgoing=true; per-SSRC sender user_id
    //   when is_outgoing=false (set when sink is registered).
    // is_outgoing: true for outbound encrypt path, false for inbound decrypt.
    E2EFrameTransformer(int64_t user_id, bool is_outgoing, E2EFrameCallback callback);

    // webrtc::FrameTransformerInterface overrides.
    void Transform(std::unique_ptr<webrtc::TransformableFrameInterface> frame) override;
    void RegisterTransformedFrameCallback(
        webrtc::scoped_refptr<webrtc::TransformedFrameCallback> callback) override;
    void UnregisterTransformedFrameCallback() override;
    void RegisterTransformedFrameSinkCallback(
        webrtc::scoped_refptr<webrtc::TransformedFrameCallback> callback,
        uint32_t ssrc) override;
    void UnregisterTransformedFrameSinkCallback(uint32_t ssrc) override;

    // Update which user_id to pass for inbound frames keyed by SSRC.
    // For outbound (single sender) the user_id given to the ctor is reused.
    void SetSenderUserId(uint32_t ssrc, int64_t user_id);

private:
    int64_t default_user_id_;
    bool is_outgoing_;
    E2EFrameCallback callback_;

    webrtc::Mutex mutex_;
    // For outgoing: a single TransformedFrameCallback (registered via
    // RegisterTransformedFrameCallback). For incoming: per-SSRC sinks.
    webrtc::scoped_refptr<webrtc::TransformedFrameCallback> default_sink_;
    std::map<uint32_t, webrtc::scoped_refptr<webrtc::TransformedFrameCallback>> ssrc_sinks_;
    std::map<uint32_t, int64_t> ssrc_user_ids_;
};

}  // namespace wrtc
