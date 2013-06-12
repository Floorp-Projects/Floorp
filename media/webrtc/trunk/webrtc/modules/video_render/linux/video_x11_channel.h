/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
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
#include "common_video/libyuv/include/webrtc_libyuv.h"
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
    VideoX11Channel(int32_t id);

    virtual ~VideoX11Channel();

    virtual int32_t RenderFrame(const uint32_t streamId,
                                I420VideoFrame& videoFrame);

    int32_t FrameSizeChange(int32_t width, int32_t height,
                            int32_t numberOfStreams);
    int32_t DeliverFrame(const I420VideoFrame& videoFrame);
    int32_t GetFrameSize(int32_t& width, int32_t& height);
    int32_t Init(Window window, float left, float top, float right,
                 float bottom);
    int32_t ChangeWindow(Window window);
    int32_t
            GetStreamProperties(uint32_t& zOrder, float& left,
                                float& top, float& right, float& bottom) const;
    int32_t ReleaseWindow();

    bool IsPrepared()
    {
        return _prepared;
    }

private:

    int32_t
            CreateLocalRenderer(int32_t width, int32_t height);
    int32_t RemoveRenderer();

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
    int32_t _width; // incoming frame width
    int32_t _height; // incoming frame height
    int32_t _outWidth; // render frame width
    int32_t _outHeight; // render frame height
    int32_t _xPos; // position within window
    int32_t _yPos;
    bool _prepared; // true if ready to use
    int32_t _dispCount;

    unsigned char* _buffer;
    float _top;
    float _left;
    float _right;
    float _bottom;

    int32_t _Id;

};


} //namespace webrtc

#endif // WEBRTC_MODULES_VIDEO_RENDER_MAIN_SOURCE_LINUX_VIDEO_X11_CHANNEL_H_
