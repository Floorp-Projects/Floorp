/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "engine_configurations.h"
#if defined(COCOA_RENDERING)

#ifndef WEBRTC_MODULES_VIDEO_RENDER_MAIN_SOURCE_MAC_VIDEO_RENDER_NSOPENGL_H_
#define WEBRTC_MODULES_VIDEO_RENDER_MAIN_SOURCE_MAC_VIDEO_RENDER_NSOPENGL_H_

#import <Cocoa/Cocoa.h>
#import <OpenGL/OpenGL.h>
#import <OpenGL/glu.h>
#import <OpenGL/glext.h>
#include <QuickTime/QuickTime.h>
#include <list>
#include <map>

#include "video_render_defines.h"

#import "cocoa_render_view.h"
#import "cocoa_full_screen_window.h"

class Trace;

namespace webrtc {
class EventWrapper;
class ThreadWrapper;
class VideoRenderNSOpenGL;
class CriticalSectionWrapper;

class VideoChannelNSOpenGL : public VideoRenderCallback
{

public:

    VideoChannelNSOpenGL(NSOpenGLContext *nsglContext, int iId, VideoRenderNSOpenGL* owner);
    virtual ~VideoChannelNSOpenGL();

    // A new frame is delivered
    virtual int DeliverFrame(const I420VideoFrame& videoFrame);

    // Called when the incomming frame size and/or number of streams in mix changes
    virtual int FrameSizeChange(int width, int height, int numberOfStreams);

    virtual int UpdateSize(int width, int height);

    // Setup
    int SetStreamSettings(int streamId, float startWidth, float startHeight, float stopWidth, float stopHeight);
    int SetStreamCropSettings(int streamId, float startWidth, float startHeight, float stopWidth, float stopHeight);

    // Called when it's time to render the last frame for the channel
    int RenderOffScreenBuffer();

    // Returns true if a new buffer has been delivered to the texture
    int IsUpdated(bool& isUpdated);
    virtual int UpdateStretchSize(int stretchHeight, int stretchWidth);

    // ********** new module functions ************ //
    virtual int32_t RenderFrame(const uint32_t streamId,
                                I420VideoFrame& videoFrame);

    // ********** new module helper functions ***** //
    int ChangeContext(NSOpenGLContext *nsglContext);
    int32_t GetChannelProperties(float& left,
                                 float& top,
                                 float& right,
                                 float& bottom);

private:

    NSOpenGLContext* _nsglContext;
    int _id;
    VideoRenderNSOpenGL* _owner;
    int32_t _width;
    int32_t _height;
    float _startWidth;
    float _startHeight;
    float _stopWidth;
    float _stopHeight;
    int _stretchedWidth;
    int _stretchedHeight;
    int _oldStretchedHeight;
    int _oldStretchedWidth;
    unsigned char* _buffer;
    int _bufferSize;
    int _incommingBufferSize;
    bool _bufferIsUpdated;
    int _numberOfStreams;
    GLenum _pixelFormat;
    GLenum _pixelDataType;
    unsigned int _texture;
};

class VideoRenderNSOpenGL
{

public: // methods
    VideoRenderNSOpenGL(CocoaRenderView *windowRef, bool fullScreen, int iId);
    ~VideoRenderNSOpenGL();

    static int GetOpenGLVersion(int& nsglMajor, int& nsglMinor);

    // Allocates textures
    int Init();
    VideoChannelNSOpenGL* CreateNSGLChannel(int streamID, int zOrder, float startWidth, float startHeight, float stopWidth, float stopHeight);
    VideoChannelNSOpenGL* ConfigureNSGLChannel(int channel, int zOrder, float startWidth, float startHeight, float stopWidth, float stopHeight);
    int DeleteNSGLChannel(int channel);
    int DeleteAllNSGLChannels();
    int StopThread();
    bool IsFullScreen();
    bool HasChannels();
    bool HasChannel(int channel);
    int GetChannels(std::list<int>& channelList);
    void LockAGLCntx();
    void UnlockAGLCntx();

    // ********** new module functions ************ //
    int ChangeWindow(CocoaRenderView* newWindowRef);
    int32_t ChangeUniqueID(int32_t id);
    int32_t StartRender();
    int32_t StopRender();
    int32_t DeleteNSGLChannel(const uint32_t streamID);
    int32_t GetChannelProperties(const uint16_t streamId,
                                 uint32_t& zOrder,
                                 float& left,
                                 float& top,
                                 float& right,
                                 float& bottom);

    int32_t SetText(const uint8_t textId,
                    const uint8_t* text,
                    const int32_t textLength,
                    const uint32_t textColorRef,
                    const uint32_t backgroundColorRef,
                    const float left,
                    const float top,
                    const float right,
                    const float bottom);

    // ********** new module helper functions ***** //
    int configureNSOpenGLEngine();
    int configureNSOpenGLView();
    int setRenderTargetWindow();
    int setRenderTargetFullScreen();

protected: // methods
    static bool ScreenUpdateThreadProc(void* obj);
    bool ScreenUpdateProcess();
    int GetWindowRect(Rect& rect);

private: // methods

    int CreateMixingContext();
    int RenderOffScreenBuffers();
    int DisplayBuffers();

private: // variables


    CocoaRenderView* _windowRef;
    bool _fullScreen;
    int _id;
    CriticalSectionWrapper& _nsglContextCritSec;
    ThreadWrapper* _screenUpdateThread;
    EventWrapper* _screenUpdateEvent;
    NSOpenGLContext* _nsglContext;
    NSOpenGLContext* _nsglFullScreenContext;
    CocoaFullScreenWindow* _fullScreenWindow;
    Rect _windowRect; // The size of the window
    int _windowWidth;
    int _windowHeight;
    std::map<int, VideoChannelNSOpenGL*> _nsglChannels;
    std::multimap<int, int> _zOrderToChannel;
    unsigned int _threadID;
    bool _renderingIsPaused;
    NSView* _windowRefSuperView;
    NSRect _windowRefSuperViewFrame;
};

} //namespace webrtc

#endif   // WEBRTC_MODULES_VIDEO_RENDER_MAIN_SOURCE_MAC_VIDEO_RENDER_NSOPENGL_H_
#endif	 // COCOA_RENDERING

