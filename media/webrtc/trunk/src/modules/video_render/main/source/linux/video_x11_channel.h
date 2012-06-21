/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_RENDER_MAIN_SOURCE_LINUX_VIDEO_X11_CHANNEL_H_
#define WEBRTC_MODULES_VIDEO_RENDER_MAIN_SOURCE_LINUX_VIDEO_X11_CHANNEL_H_

#include "video_render_defines.h"
#include "common_video/libyuv/include/libyuv.h"
#include <sys/shm.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/XShm.h>

namespace webrtc {
class CriticalSectionWrapper;

#define DEFAULT_RENDER_FRAME_WIDTH 352
#define DEFAULT_RENDER_FRAME_HEIGHT 288


class VideoX11Channel: public VideoRenderCallback
{
public:
    VideoX11Channel(WebRtc_Word32 id);

    virtual ~VideoX11Channel();

    virtual WebRtc_Word32 RenderFrame(const WebRtc_UWord32 streamId,
                                      VideoFrame& videoFrame);

    WebRtc_Word32 FrameSizeChange(WebRtc_Word32 width, WebRtc_Word32 height,
                                  WebRtc_Word32 numberOfStreams);
    WebRtc_Word32 DeliverFrame(unsigned char* buffer, WebRtc_Word32 bufferSize,
                               unsigned WebRtc_Word32 /*timeStamp90kHz*/);
    WebRtc_Word32 GetFrameSize(WebRtc_Word32& width, WebRtc_Word32& height);
    WebRtc_Word32 Init(Window window, float left, float top, float right,
                       float bottom);
    WebRtc_Word32 ChangeWindow(Window window);
    WebRtc_Word32
            GetStreamProperties(WebRtc_UWord32& zOrder, float& left,
                                float& top, float& right, float& bottom) const;
    WebRtc_Word32 ReleaseWindow();

    bool IsPrepared()
    {
        return _prepared;
    }

private:

    WebRtc_Word32
            CreateLocalRenderer(WebRtc_Word32 width, WebRtc_Word32 height);
    WebRtc_Word32 RemoveRenderer();

    //FIXME a better place for this method? the GetWidthHeight no longer
    // supported by common_video.
    int GetWidthHeight(VideoType type, int bufferSize, int& width,
                       int& height);

    CriticalSectionWrapper& _crit;

    Display* _display;
    XShmSegmentInfo _shminfo;
    XImage* _image;
    Window _window;
    GC _gc;
    WebRtc_Word32 _width; // incoming frame width
    WebRtc_Word32 _height; // incoming frame height
    WebRtc_Word32 _outWidth; // render frame width
    WebRtc_Word32 _outHeight; // render frame height
    WebRtc_Word32 _xPos; // position within window
    WebRtc_Word32 _yPos;
    bool _prepared; // true if ready to use
    WebRtc_Word32 _dispCount;

    unsigned char* _buffer;
    float _top;
    float _left;
    float _right;
    float _bottom;

    WebRtc_Word32 _Id;

};


} //namespace webrtc

#endif // WEBRTC_MODULES_VIDEO_RENDER_MAIN_SOURCE_LINUX_VIDEO_X11_CHANNEL_H_
