/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/video_render/linux/video_x11_channel.h"

#include "webrtc/system_wrappers/include/critical_section_wrapper.h"
#include "webrtc/system_wrappers/include/trace.h"

namespace webrtc {

#define DISP_MAX 128

static Display *dispArray[DISP_MAX];
static int dispCount = 0;


VideoX11Channel::VideoX11Channel(int32_t id) :
    _crit(*CriticalSectionWrapper::CreateCriticalSection()), _display(NULL),
          _shminfo(), _image(NULL), _window(0L), _gc(NULL),
          _width(DEFAULT_RENDER_FRAME_WIDTH),
          _height(DEFAULT_RENDER_FRAME_HEIGHT), _outWidth(0), _outHeight(0),
          _xPos(0), _yPos(0), _prepared(false), _dispCount(0), _buffer(NULL),
          _top(0.0), _left(0.0), _right(0.0), _bottom(0.0),
          _Id(id)
{
}

VideoX11Channel::~VideoX11Channel()
{
    if (_prepared)
    {
        _crit.Enter();
        ReleaseWindow();
        _crit.Leave();
    }
    delete &_crit;
}

int32_t VideoX11Channel::RenderFrame(const uint32_t streamId,
                                     const VideoFrame& videoFrame) {
  CriticalSectionScoped cs(&_crit);
  if (_width != videoFrame.width() || _height
      != videoFrame.height()) {
      if (FrameSizeChange(videoFrame.width(), videoFrame.height(), 1) == -1) {
        return -1;
    }
  }
  return DeliverFrame(videoFrame);
}

int32_t VideoX11Channel::FrameSizeChange(int32_t width,
                                         int32_t height,
                                         int32_t /*numberOfStreams */)
{
    CriticalSectionScoped cs(&_crit);
    if (_prepared)
    {
        RemoveRenderer();
    }
    if (CreateLocalRenderer(width, height) == -1)
    {
        return -1;
    }

    return 0;
}

int32_t VideoX11Channel::DeliverFrame(const VideoFrame& videoFrame) {
  CriticalSectionScoped cs(&_crit);
  if (!_prepared) {
    return 0;
  }

  if (!dispArray[_dispCount]) {
    return -1;
  }

  ConvertFromI420(videoFrame, kARGB, 0, _buffer);

  // Put image in window.
  XShmPutImage(_display, _window, _gc, _image, 0, 0, _xPos, _yPos, _width,
               _height, True);

  // Very important for the image to update properly!
  XSync(_display, False);
  return 0;
}

int32_t VideoX11Channel::GetFrameSize(int32_t& width, int32_t& height)
{
    width = _width;
    height = _height;

    return 0;
}

int32_t VideoX11Channel::Init(Window window, float left, float top,
                              float right, float bottom)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVideoRenderer, _Id, "%s",
                 __FUNCTION__);
    CriticalSectionScoped cs(&_crit);

    _window = window;
    _left = left;
    _right = right;
    _top = top;
    _bottom = bottom;

    _display = XOpenDisplay(NULL); // Use default display
    if (!_window || !_display)
    {
        return -1;
    }

    if (dispCount < DISP_MAX)
    {
        dispArray[dispCount] = _display;
        _dispCount = dispCount;
        dispCount++;
    }
    else
    {
        return -1;
    }

    if ((1 < left || left < 0) || (1 < top || top < 0) || (1 < right || right
            < 0) || (1 < bottom || bottom < 0))
    {
        return -1;
    }

    // calculate position and size of rendered video
    int x, y;
    unsigned int winWidth, winHeight, borderwidth, depth;
    Window rootret;
    if (XGetGeometry(_display, _window, &rootret, &x, &y, &winWidth,
                     &winHeight, &borderwidth, &depth) == 0)
    {
        return -1;
    }

    _xPos = (int32_t) (winWidth * left);
    _yPos = (int32_t) (winHeight * top);
    _outWidth = (int32_t) (winWidth * (right - left));
    _outHeight = (int32_t) (winHeight * (bottom - top));
    if (_outWidth % 2)
        _outWidth++; // the renderer want's sizes that are multiples of two
    if (_outHeight % 2)
        _outHeight++;

    _gc = XCreateGC(_display, _window, 0, 0);
    if (!_gc) {
      // Failed to create the graphics context.
      assert(false);
      return -1;
    }

    if (CreateLocalRenderer(winWidth, winHeight) == -1)
    {
        return -1;
    }
    return 0;

}

int32_t VideoX11Channel::ChangeWindow(Window window)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVideoRenderer, _Id, "%s",
                 __FUNCTION__);
    CriticalSectionScoped cs(&_crit);

    // Stop the rendering, if we are rendering...
    RemoveRenderer();
    _window = window;

    // calculate position and size of rendered video
    int x, y;
    unsigned int winWidth, winHeight, borderwidth, depth;
    Window rootret;
    if (XGetGeometry(_display, _window, &rootret, &x, &y, &winWidth,
                     &winHeight, &borderwidth, &depth) == -1)
    {
        return -1;
    }
    _xPos = (int) (winWidth * _left);
    _yPos = (int) (winHeight * _top);
    _outWidth = (int) (winWidth * (_right - _left));
    _outHeight = (int) (winHeight * (_bottom - _top));
    if (_outWidth % 2)
        _outWidth++; // the renderer want's sizes that are multiples of two
    if (_outHeight % 2)
        _outHeight++;

    // Prepare rendering using the
    if (CreateLocalRenderer(_width, _height) == -1)
    {
        return -1;
    }
    return 0;
}

int32_t VideoX11Channel::ReleaseWindow()
{
    WEBRTC_TRACE(kTraceInfo, kTraceVideoRenderer, _Id, "%s",
                 __FUNCTION__);
    CriticalSectionScoped cs(&_crit);

    RemoveRenderer();
    if (_gc) {
      XFreeGC(_display, _gc);
      _gc = NULL;
    }
    if (_display)
    {
        XCloseDisplay(_display);
        _display = NULL;
    }
    return 0;
}

int32_t VideoX11Channel::CreateLocalRenderer(int32_t width, int32_t height)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVideoRenderer, _Id, "%s",
                 __FUNCTION__);
    CriticalSectionScoped cs(&_crit);

    if (!_window || !_display)
    {
        return -1;
    }

    if (_prepared)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceVideoRenderer, _Id,
                     "Renderer already prepared, exits.");
        return -1;
    }

    _width = width;
    _height = height;

    // create shared memory image
    _image = XShmCreateImage(_display, CopyFromParent, 24, ZPixmap, NULL,
                             &_shminfo, _width, _height); // this parameter needs to be the same for some reason.
    _shminfo.shmid = shmget(IPC_PRIVATE, (_image->bytes_per_line
            * _image->height), IPC_CREAT | 0777);
    _shminfo.shmaddr = _image->data = (char*) shmat(_shminfo.shmid, 0, 0);
    if (_image->data == reinterpret_cast<char*>(-1))
    {
        return -1;
    }
    _buffer = (unsigned char*) _image->data;
    _shminfo.readOnly = False;

    // attach image to display
    if (!XShmAttach(_display, &_shminfo))
    {
        //printf("XShmAttach failed !\n");
        return -1;
    }
    XSync(_display, False);

    _prepared = true;
    return 0;
}

int32_t VideoX11Channel::RemoveRenderer()
{
    WEBRTC_TRACE(kTraceInfo, kTraceVideoRenderer, _Id, "%s",
                 __FUNCTION__);

    if (!_prepared)
    {
        return 0;
    }
    _prepared = false;

    // Free the memory.
    XShmDetach(_display, &_shminfo);
    XDestroyImage( _image );
    _image = NULL;
    shmdt(_shminfo.shmaddr);
    _shminfo.shmaddr = NULL;
    _buffer = NULL;
    shmctl(_shminfo.shmid, IPC_RMID, 0);
    _shminfo.shmid = 0;
    return 0;
}

int32_t VideoX11Channel::GetStreamProperties(uint32_t& zOrder,
                                             float& left, float& top,
                                             float& right, float& bottom) const
{
    WEBRTC_TRACE(kTraceInfo, kTraceVideoRenderer, _Id, "%s",
                 __FUNCTION__);

    zOrder = 0; // no z-order support yet
    left = _left;
    top = _top;
    right = _right;
    bottom = _bottom;

    return 0;
}


}  // namespace webrtc
