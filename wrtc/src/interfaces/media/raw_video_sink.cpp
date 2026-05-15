//
// Created by Laky64 on 26/10/24.
//

#include <wrtc/interfaces/media/raw_video_sink.hpp>
#include <cstdio>

namespace wrtc {
    RawVideoSink::~RawVideoSink() {
        callbackData = nullptr;
    }

    void RawVideoSink::OnFrame(const webrtc::VideoFrame& frame) {
        static thread_local int _dbb_frameCounter = 0;
        ++_dbb_frameCounter;
        if (_dbb_frameCounter <= 10 || (_dbb_frameCounter % 100) == 0) {
            fprintf(stderr, "[dialogbrain-diag] RawVideoSink::OnFrame ssrc=%u w=%d h=%d n=%d hasCb=%d\n",
                    ssrc, frame.width(), frame.height(), _dbb_frameCounter, callbackData ? 1 : 0);
            fflush(stderr);
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