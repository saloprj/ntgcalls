//
// Created by Laky64 on 26/10/24.
//

#include <wrtc/interfaces/media/raw_video_sink.hpp>
#include <rtc_base/logging.h>

namespace wrtc {
    RawVideoSink::~RawVideoSink() {
        callbackData = nullptr;
    }

    void RawVideoSink::OnFrame(const webrtc::VideoFrame& frame) {
        static thread_local int _dbb_frameCounter = 0;
        ++_dbb_frameCounter;
        if (_dbb_frameCounter <= 10 || (_dbb_frameCounter % 100) == 0) {
            RTC_LOG(LS_INFO) << "[dialogbrain-diag] RawVideoSink::OnFrame ssrc=" << ssrc
                              << " w=" << frame.width()
                              << " h=" << frame.height()
                              << " n=" << _dbb_frameCounter
                              << " hasCb=" << (callbackData ? 1 : 0);
        }
        if (callbackData) {
            callbackData(ssrc, std::make_unique<webrtc::VideoFrame>(frame));
        }
    }

    void RawVideoSink::setRemoteVideoSink(const uint32_t ssrc, std::function<void(uint32_t, std::unique_ptr<webrtc::VideoFrame>)> callback) {
        callbackData = std::move(callback);
        this->ssrc = ssrc;
    }
} // wrtc