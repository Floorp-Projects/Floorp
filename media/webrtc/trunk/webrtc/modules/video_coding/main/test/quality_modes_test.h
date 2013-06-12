/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_CODING_TEST_QUALITY_MODSE_TEST_H_
#define WEBRTC_MODULES_VIDEO_CODING_TEST_QUALITY_MODSE_TEST_H_

#include "video_processing.h"
#include "normal_test.h"
#include "system_wrappers/interface/data_log.h"
#include "video_coding_defines.h"

int qualityModeTest(const CmdArgs& args);

class QualityModesTest : public NormalTest
{
public:
    QualityModesTest(webrtc::VideoCodingModule* vcm,
                     webrtc::TickTimeBase* clock);
    virtual ~QualityModesTest();
    WebRtc_Word32 Perform(const CmdArgs& args);

private:

    void Setup(const CmdArgs& args);
    void Print();
    void Teardown();
    void SsimComp();

    webrtc::VideoProcessingModule*  _vpm;

    int                                 _width;
    int                                 _height;
    float                               _frameRate;
    int                                 _nativeWidth;
    int                                 _nativeHeight;
    float                               _nativeFrameRate;

    WebRtc_UWord32                      _numFramesDroppedVPM;
    bool                                _flagSSIM;
    std::string                         filename_testvideo_;
    std::string                         fv_outfilename_;

    std::string                         feature_table_name_;

}; // end of QualityModesTest class

class VCMQMDecodeCompleCallback: public webrtc::VCMReceiveCallback
{
public:
    VCMQMDecodeCompleCallback(
        FILE* decodedFile,
        int frame_rate,
        std::string feature_table_name);
    virtual ~VCMQMDecodeCompleCallback();
    void SetUserReceiveCallback(webrtc::VCMReceiveCallback* receiveCallback);
    // will write decoded frame into file
    WebRtc_Word32 FrameToRender(webrtc::I420VideoFrame& videoFrame);
    WebRtc_Word32 DecodedBytes();
    void SetOriginalFrameDimensions(WebRtc_Word32 width, WebRtc_Word32 height);
    WebRtc_Word32 buildInterpolator();
    // Check if last frame is dropped, if so, repeat the last rendered frame.
    void WriteEnd(int input_tot_frame_count);

private:
    FILE*                _decodedFile;
    WebRtc_UWord32       _decodedBytes;
   // QualityModesTest&  _test;
    int                  _origWidth;
    int                  _origHeight;
    int                  _decWidth;
    int                  _decHeight;
//    VideoInterpolator* _interpolator;
    WebRtc_UWord8*       _decBuffer;
    WebRtc_UWord32       _frameCnt; // debug
    webrtc::I420VideoFrame last_frame_;
    int                  frame_rate_;
    int                  frames_cnt_since_drop_;
    std::string          feature_table_name_;



}; // end of VCMQMDecodeCompleCallback class

class QMTestVideoSettingsCallback : public webrtc::VCMQMSettingsCallback
{
public:
    QMTestVideoSettingsCallback();
    // update VPM with QM settings
    WebRtc_Word32 SetVideoQMSettings(const WebRtc_UWord32 frameRate,
                                     const WebRtc_UWord32 width,
                                     const WebRtc_UWord32 height);
    // register VPM used by test
    void RegisterVPM(webrtc::VideoProcessingModule* vpm);
    void RegisterVCM(webrtc::VideoCodingModule* vcm);
    bool Updated();

private:
    webrtc::VideoProcessingModule*         _vpm;
    webrtc::VideoCodingModule*             _vcm;
    bool                                   _updated;
};


#endif // WEBRTC_MODULES_VIDEO_CODING_TEST_QUALITY_MODSE_TEST_H_
