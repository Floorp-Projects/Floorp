/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "engine_configurations.h"

#if defined(CARBON_RENDERING)

#ifndef WEBRTC_MODULES_VIDEO_RENDER_MAIN_SOURCE_MAC_VIDEO_RENDER_AGL_H_
#define WEBRTC_MODULES_VIDEO_RENDER_MAIN_SOURCE_MAC_VIDEO_RENDER_AGL_H_

#include "video_render_defines.h"

#define NEW_HIVIEW_PARENT_EVENT_HANDLER 1
#define NEW_HIVIEW_EVENT_HANDLER 1
#define USE_STRUCT_RGN

#include <AGL/agl.h>
#include <Carbon/Carbon.h>
#include <OpenGL/OpenGL.h>
#include <OpenGL/glu.h>
#include <OpenGL/glext.h>
#include <list>
#include <map>

class VideoRenderAGL;

namespace webrtc {
class CriticalSectionWrapper;
class EventWrapper;
class ThreadWrapper;

class VideoChannelAGL : public VideoRenderCallback {
 public:

  VideoChannelAGL(AGLContext& aglContext, int iId, VideoRenderAGL* owner);
  virtual ~VideoChannelAGL();
  virtual int FrameSizeChange(int width, int height, int numberOfStreams);
  virtual int DeliverFrame(const I420VideoFrame& videoFrame);
  virtual int UpdateSize(int width, int height);
  int SetStreamSettings(int streamId, float startWidth, float startHeight,
                        float stopWidth, float stopHeight);
  int SetStreamCropSettings(int streamId, float startWidth, float startHeight,
                            float stopWidth, float stopHeight);
  int RenderOffScreenBuffer();
  int IsUpdated(bool& isUpdated);
  virtual int UpdateStretchSize(int stretchHeight, int stretchWidth);
  virtual int32_t RenderFrame(const uint32_t streamId,
                              I420VideoFrame& videoFrame);

 private:

  AGLContext _aglContext;
  int _id;
  VideoRenderAGL* _owner;
  int _width;
  int _height;
  int _stretchedWidth;
  int _stretchedHeight;
  float _startHeight;
  float _startWidth;
  float _stopWidth;
  float _stopHeight;
  int _xOldWidth;
  int _yOldHeight;
  int _oldStretchedHeight;
  int _oldStretchedWidth;
  unsigned char* _buffer;
  int _bufferSize;
  int _incommingBufferSize;
  bool _bufferIsUpdated;
  bool _sizeInitialized;
  int _numberOfStreams;
  bool _bVideoSizeStartedChanging;
  GLenum _pixelFormat;
  GLenum _pixelDataType;
  unsigned int _texture;
};

class VideoRenderAGL {
 public:
  VideoRenderAGL(WindowRef windowRef, bool fullscreen, int iId);
  VideoRenderAGL(HIViewRef windowRef, bool fullscreen, int iId);
  ~VideoRenderAGL();

  int Init();
  VideoChannelAGL* CreateAGLChannel(int channel, int zOrder, float startWidth,
                                    float startHeight, float stopWidth,
                                    float stopHeight);
  VideoChannelAGL* ConfigureAGLChannel(int channel, int zOrder,
                                       float startWidth, float startHeight,
                                       float stopWidth, float stopHeight);
  int DeleteAGLChannel(int channel);
  int DeleteAllAGLChannels();
  int StopThread();
  bool IsFullScreen();
  bool HasChannels();
  bool HasChannel(int channel);
  int GetChannels(std::list<int>& channelList);
  void LockAGLCntx();
  void UnlockAGLCntx();

  static int GetOpenGLVersion(int& aglMajor, int& aglMinor);

  // ********** new module functions ************ //
  int ChangeWindow(void* newWindowRef);
  int32_t ChangeUniqueID(int32_t id);
  int32_t StartRender();
  int32_t StopRender();
  int32_t DeleteAGLChannel(const uint32_t streamID);
  int32_t GetChannelProperties(const uint16_t streamId, uint32_t& zOrder,
                               float& left, float& top, float& right,
                               float& bottom);

 protected:
  static bool ScreenUpdateThreadProc(void* obj);
  bool ScreenUpdateProcess();
  int GetWindowRect(Rect& rect);

 private:
  int CreateMixingContext();
  int RenderOffScreenBuffers();
  int SwapAndDisplayBuffers();
  int UpdateClipping();
  int CalculateVisibleRegion(ControlRef control, RgnHandle& visibleRgn,
                             bool clipChildren);
  bool CheckValidRegion(RgnHandle rHandle);
  void ParentWindowResized(WindowRef window);

  // Carbon GUI event handlers
  static pascal OSStatus sHandleWindowResized(
      EventHandlerCallRef nextHandler, EventRef theEvent, void* userData);
  static pascal OSStatus sHandleHiViewResized(
      EventHandlerCallRef nextHandler, EventRef theEvent, void* userData);

  HIViewRef _hiviewRef;
  WindowRef _windowRef;
  bool _fullScreen;
  int _id;
  webrtc::CriticalSectionWrapper& _renderCritSec;
  webrtc::ThreadWrapper* _screenUpdateThread;
  webrtc::EventWrapper* _screenUpdateEvent;
  bool _isHIViewRef;
  AGLContext _aglContext;
  int _windowWidth;
  int _windowHeight;
  int _lastWindowWidth;
  int _lastWindowHeight;
  int _lastHiViewWidth;
  int _lastHiViewHeight;
  int _currentParentWindowHeight;
  int _currentParentWindowWidth;
  Rect _currentParentWindowBounds;
  bool _windowHasResized;
  Rect _lastParentWindowBounds;
  Rect _currentHIViewBounds;
  Rect _lastHIViewBounds;
  Rect _windowRect;
  std::map<int, VideoChannelAGL*> _aglChannels;
  std::multimap<int, int> _zOrderToChannel;
  EventHandlerRef _hiviewEventHandlerRef;
  EventHandlerRef _windowEventHandlerRef;
  HIRect _currentViewBounds;
  HIRect _lastViewBounds;
  bool _renderingIsPaused;
  unsigned int _threadID;

};

}  //namespace webrtc

#endif  // WEBRTC_MODULES_VIDEO_RENDER_MAIN_SOURCE_MAC_VIDEO_RENDER_AGL_H_

#endif  // CARBON_RENDERING
