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

#if defined(CARBON_RENDERING)

#include "video_render_agl.h"

//  includes
#include "critical_section_wrapper.h"
#include "event_wrapper.h"
#include "trace.h"
#include "thread_wrapper.h"
#include "common_video/libyuv/include/webrtc_libyuv.h"

namespace webrtc {

/*
 *
 *    VideoChannelAGL
 *
 */

#pragma mark VideoChannelAGL constructor

VideoChannelAGL::VideoChannelAGL(AGLContext& aglContext, int iId, VideoRenderAGL* owner) :
    _aglContext( aglContext),
    _id( iId),
    _owner( owner),
    _width( 0),
    _height( 0),
    _stretchedWidth( 0),
    _stretchedHeight( 0),
    _startWidth( 0.0f),
    _startHeight( 0.0f),
    _stopWidth( 0.0f),
    _stopHeight( 0.0f),
    _xOldWidth( 0),
    _yOldHeight( 0),
    _oldStretchedHeight(0),
    _oldStretchedWidth( 0),
    _buffer( 0),
    _bufferSize( 0),
    _incommingBufferSize(0),
    _bufferIsUpdated( false),
    _sizeInitialized( false),
    _numberOfStreams( 0),
    _bVideoSizeStartedChanging(false),
    _pixelFormat( GL_RGBA),
    _pixelDataType( GL_UNSIGNED_INT_8_8_8_8),
    _texture( 0)

{
    //WEBRTC_TRACE(kTraceInfo, kTraceVideoRenderer, _id, "%s:%d Constructor", __FUNCTION__, __LINE__);
}

VideoChannelAGL::~VideoChannelAGL()
{
    //WEBRTC_TRACE(kTraceInfo, kTraceVideoRenderer, _id, "%s:%d Destructor", __FUNCTION__, __LINE__);
    if (_buffer)
    {
        delete [] _buffer;
        _buffer = NULL;
    }

    aglSetCurrentContext(_aglContext);

    if (_texture != 0)
    {
        glDeleteTextures(1, (const GLuint*) &_texture);
        _texture = 0;
    }
}

WebRtc_Word32 VideoChannelAGL::RenderFrame(const WebRtc_UWord32 streamId,
                                           I420VideoFrame& videoFrame) {
  _owner->LockAGLCntx();
  if (_width != videoFrame.width() ||
      _height != videoFrame.height()) {
    if (FrameSizeChange(videoFrame.width(), videoFrame.height(), 1) == -1) {
      WEBRTC_TRACE(kTraceError, kTraceVideoRenderer, _id, "%s:%d FrameSize
                   Change returned an error", __FUNCTION__, __LINE__);
      _owner->UnlockAGLCntx();
      return -1;
    }
  }

  _owner->UnlockAGLCntx();
  return DeliverFrame(videoFrame);
}

int VideoChannelAGL::UpdateSize(int /*width*/, int /*height*/)
{
    _owner->LockAGLCntx();
    _owner->UnlockAGLCntx();
    return 0;
}

int VideoChannelAGL::UpdateStretchSize(int stretchHeight, int stretchWidth)
{

    _owner->LockAGLCntx();
    _stretchedHeight = stretchHeight;
    _stretchedWidth = stretchWidth;
    _owner->UnlockAGLCntx();
    return 0;
}

int VideoChannelAGL::FrameSizeChange(int width, int height, int numberOfStreams)
{
    //  We'll get a new frame size from VideoAPI, prepare the buffer

    _owner->LockAGLCntx();

    if (width == _width && _height == height)
    {
        // We already have a correct buffer size
        _numberOfStreams = numberOfStreams;
        _owner->UnlockAGLCntx();
        return 0;
    }

    _width = width;
    _height = height;

    // Delete the old buffer, create a new one with correct size.
    if (_buffer)
    {
        delete [] _buffer;
        _bufferSize = 0;
    }

    _incommingBufferSize = CalcBufferSize(kI420, _width, _height);
    _bufferSize = CalcBufferSize(kARGB, _width, _height);//_width * _height * bytesPerPixel;
    _buffer = new unsigned char [_bufferSize];
    memset(_buffer, 0, _bufferSize * sizeof(unsigned char));

    if (aglSetCurrentContext(_aglContext) == false)
    {
        _owner->UnlockAGLCntx();
        return -1;
    }

    // Delete a possible old texture
    if (_texture != 0)
    {
        glDeleteTextures(1, (const GLuint*) &_texture);
        _texture = 0;
    }

    // Create a new texture
    glGenTextures(1, (GLuint *) &_texture);

    GLenum glErr = glGetError();

    if (glErr != GL_NO_ERROR)
    {
    }

    // Do the setup for both textures
    // Note: we setup two textures even if we're not running full screen
    glBindTexture(GL_TEXTURE_RECTANGLE_EXT, _texture);

    // Set texture parameters
    glTexParameterf(GL_TEXTURE_RECTANGLE_EXT, GL_TEXTURE_PRIORITY, 1.0);

    glTexParameteri(GL_TEXTURE_RECTANGLE_EXT, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_RECTANGLE_EXT, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glTexParameteri(GL_TEXTURE_RECTANGLE_EXT, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_RECTANGLE_EXT, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    //glTexParameteri(GL_TEXTURE_RECTANGLE_EXT, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    //glTexParameteri(GL_TEXTURE_RECTANGLE_EXT, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    glTexParameteri(GL_TEXTURE_RECTANGLE_EXT, GL_TEXTURE_STORAGE_HINT_APPLE, GL_STORAGE_SHARED_APPLE);

    // Maximum width/height for a texture
    GLint texSize;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &texSize);

    if (texSize < _width || texSize < _height)
    {
        // Image too big for memory
        _owner->UnlockAGLCntx();
        return -1;
    }

    // Set up th texture type and size
    glTexImage2D(GL_TEXTURE_RECTANGLE_EXT, // target
            0, // level
            GL_RGBA, // internal format
            _width, // width
            _height, // height
            0, // border 0/1 = off/on
            _pixelFormat, // format, GL_BGRA
            _pixelDataType, // data type, GL_UNSIGNED_INT_8_8_8_8
            _buffer); // pixel data

    glErr = glGetError();
    if (glErr != GL_NO_ERROR)
    {
        _owner->UnlockAGLCntx();
        return -1;
    }

    _owner->UnlockAGLCntx();
    return 0;
}

// Called from video engine when a new frame should be rendered.
int VideoChannelAGL::DeliverFrame(const I420VideoFrame& videoFrame) {
  _owner->LockAGLCntx();

  if (_texture == 0) {
    _owner->UnlockAGLCntx();
    return 0;
  }

  int length = CalcBufferSize(kI420, videoFrame.width(), videoFrame.height());
  if (length != _incommingBufferSize) {
    _owner->UnlockAGLCntx();
    return -1;
  }

  // Setting stride = width.
  int rgbret = ConvertFromYV12(videoFrame, kBGRA, 0, _buffer);
  if (rgbret < 0) {
    _owner->UnlockAGLCntx();
    return -1;
  }

  aglSetCurrentContext(_aglContext);

  // Put the new frame into the graphic card texture.
  // Make sure this texture is the active one
  glBindTexture(GL_TEXTURE_RECTANGLE_EXT, _texture);
  GLenum glErr = glGetError();
  if (glErr != GL_NO_ERROR) {
    _owner->UnlockAGLCntx();
    return -1;
  }

  // Copy buffer to texture
  glTexSubImage2D(GL_TEXTURE_RECTANGLE_EXT,
                  0, // Level, not use
                  0, // start point x, (low left of pic)
                  0, // start point y,
                  _width, // width
                  _height, // height
                  _pixelFormat, // pictue format for _buffer
                  _pixelDataType, // data type of _buffer
                  (const GLvoid*) _buffer); // the pixel data

  if (glGetError() != GL_NO_ERROR) {
    _owner->UnlockAGLCntx();
    return -1;
  }

  _bufferIsUpdated = true;
  _owner->UnlockAGLCntx();

  return 0;
}

int VideoChannelAGL::RenderOffScreenBuffer()
{

    _owner->LockAGLCntx();

    if (_texture == 0)
    {
        _owner->UnlockAGLCntx();
        return 0;
    }

    GLfloat xStart = 2.0f * _startWidth - 1.0f;
    GLfloat xStop = 2.0f * _stopWidth - 1.0f;
    GLfloat yStart = 1.0f - 2.0f * _stopHeight;
    GLfloat yStop = 1.0f - 2.0f * _startHeight;

    aglSetCurrentContext(_aglContext);
    glBindTexture(GL_TEXTURE_RECTANGLE_EXT, _texture);

    if(_stretchedWidth != _oldStretchedWidth || _stretchedHeight != _oldStretchedHeight)
    {
        glViewport(0, 0, _stretchedWidth, _stretchedHeight);
    }
    _oldStretchedHeight = _stretchedHeight;
    _oldStretchedWidth = _stretchedWidth;

    // Now really put the texture into the framebuffer
    glLoadIdentity();

    glEnable(GL_TEXTURE_RECTANGLE_EXT);

    glBegin(GL_POLYGON);
    {
        glTexCoord2f(0.0, 0.0); glVertex2f(xStart, yStop);
        glTexCoord2f(_width, 0.0); glVertex2f(xStop, yStop);
        glTexCoord2f(_width, _height); glVertex2f(xStop, yStart);
        glTexCoord2f(0.0, _height); glVertex2f(xStart, yStart);
    }
    glEnd();

    glDisable(GL_TEXTURE_RECTANGLE_EXT);

    _bufferIsUpdated = false;

    _owner->UnlockAGLCntx();
    return 0;
}

int VideoChannelAGL::IsUpdated(bool& isUpdated)
{
    _owner->LockAGLCntx();
    isUpdated = _bufferIsUpdated;
    _owner->UnlockAGLCntx();

    return 0;
}

int VideoChannelAGL::SetStreamSettings(int /*streamId*/, float startWidth, float startHeight, float stopWidth, float stopHeight)
{

    _owner->LockAGLCntx();

    _startWidth = startWidth;
    _stopWidth = stopWidth;
    _startHeight = startHeight;
    _stopHeight = stopHeight;

    int oldWidth = _width;
    int oldHeight = _height;
    int oldNumberOfStreams = _numberOfStreams;

    _width = 0;
    _height = 0;

    int retVal = FrameSizeChange(oldWidth, oldHeight, oldNumberOfStreams);

    _owner->UnlockAGLCntx();

    return retVal;
}

int VideoChannelAGL::SetStreamCropSettings(int /*streamId*/, float /*startWidth*/, float /*startHeight*/, float /*stopWidth*/, float /*stopHeight*/)
{
    return -1;
}

#pragma mark VideoRenderAGL WindowRef constructor

VideoRenderAGL::VideoRenderAGL(WindowRef windowRef, bool fullscreen, int iId) :
_hiviewRef( 0),
_windowRef( windowRef),
_fullScreen( fullscreen),
_id( iId),
_renderCritSec(*CriticalSectionWrapper::CreateCriticalSection()),
_screenUpdateThread( 0),
_screenUpdateEvent( 0),
_isHIViewRef( false),
_aglContext( 0),
_windowWidth( 0),
_windowHeight( 0),
_lastWindowWidth( -1),
_lastWindowHeight( -1),
_lastHiViewWidth( -1),
_lastHiViewHeight( -1),
_currentParentWindowHeight( 0),
_currentParentWindowWidth( 0),
_currentParentWindowBounds( ),
_windowHasResized( false),
_lastParentWindowBounds( ),
_currentHIViewBounds( ),
_lastHIViewBounds( ),
_windowRect( ),
_aglChannels( ),
_zOrderToChannel( ),
_hiviewEventHandlerRef( NULL),
_windowEventHandlerRef( NULL),
_currentViewBounds( ),
_lastViewBounds( ),
_renderingIsPaused( false),
_threadID( )

{
    //WEBRTC_TRACE(kTraceInfo, kTraceVideoRenderer, _id, "%s");

    _screenUpdateThread = ThreadWrapper::CreateThread(ScreenUpdateThreadProc, this, kRealtimePriority);
    _screenUpdateEvent = EventWrapper::Create();

    if(!IsValidWindowPtr(_windowRef))
    {
        //WEBRTC_TRACE(kTraceError, kTraceVideoRenderer, _id, "%s:%d Invalid WindowRef:0x%x", __FUNCTION__, __LINE__, _windowRef);
    }
    else
    {
        //WEBRTC_TRACE(kTraceDebug, kTraceVideoRenderer, _id, "%s:%d WindowRef 0x%x is valid", __FUNCTION__, __LINE__, _windowRef);
    }

    GetWindowRect(_windowRect);

    _lastViewBounds.origin.x = 0;
    _lastViewBounds.origin.y = 0;
    _lastViewBounds.size.width = 0;
    _lastViewBounds.size.height = 0;

}

// this is a static function. It has been registered (in class constructor) to be called on various window redrawing or resizing.
// Since it is a static method, I have passed in "this" as the userData (one and only allowed) parameter, then calling member methods on it.
#pragma mark WindowRef Event Handler
pascal OSStatus VideoRenderAGL::sHandleWindowResized (EventHandlerCallRef /*nextHandler*/,
        EventRef theEvent,
        void* userData)
{
    WindowRef windowRef = NULL;

    int eventType = GetEventKind(theEvent);

    // see https://dcs.sourcerepo.com/dcs/tox_view/trunk/tox/libraries/i686-win32/include/quicktime/CarbonEvents.h for a list of codes
    GetEventParameter (theEvent,
            kEventParamDirectObject,
            typeWindowRef,
            NULL,
            sizeof (WindowRef),
            NULL,
            &windowRef);

    VideoRenderAGL* obj = (VideoRenderAGL*)(userData);

    bool updateUI = true;
    if(kEventWindowBoundsChanged == eventType)
    {
    }
    else if(kEventWindowBoundsChanging == eventType)
    {
    }
    else if(kEventWindowZoomed == eventType)
    {
    }
    else if(kEventWindowExpanding == eventType)
    {
    }
    else if(kEventWindowExpanded == eventType)
    {
    }
    else if(kEventWindowClickResizeRgn == eventType)
    {
    }
    else if(kEventWindowClickDragRgn == eventType)
    {
    }
    else
    {
        updateUI = false;
    }

    if(true == updateUI)
    {
        obj->ParentWindowResized(windowRef);
        obj->UpdateClipping();
        obj->RenderOffScreenBuffers();
    }

    return noErr;
}

#pragma mark VideoRenderAGL HIViewRef constructor

VideoRenderAGL::VideoRenderAGL(HIViewRef windowRef, bool fullscreen, int iId) :
_hiviewRef( windowRef),
_windowRef( 0),
_fullScreen( fullscreen),
_id( iId),
_renderCritSec(*CriticalSectionWrapper::CreateCriticalSection()),
_screenUpdateThread( 0),
_screenUpdateEvent( 0),
_isHIViewRef( false),
_aglContext( 0),
_windowWidth( 0),
_windowHeight( 0),
_lastWindowWidth( -1),
_lastWindowHeight( -1),
_lastHiViewWidth( -1),
_lastHiViewHeight( -1),
_currentParentWindowHeight( 0),
_currentParentWindowWidth( 0),
_currentParentWindowBounds( ),
_windowHasResized( false),
_lastParentWindowBounds( ),
_currentHIViewBounds( ),
_lastHIViewBounds( ),
_windowRect( ),
_aglChannels( ),
_zOrderToChannel( ),
_hiviewEventHandlerRef( NULL),
_windowEventHandlerRef( NULL),
_currentViewBounds( ),
_lastViewBounds( ),
_renderingIsPaused( false),
_threadID( )
{
    //WEBRTC_TRACE(kTraceDebug, "%s:%d Constructor", __FUNCTION__, __LINE__);
    //    _renderCritSec = CriticalSectionWrapper::CreateCriticalSection();

    _screenUpdateThread = ThreadWrapper::CreateThread(ScreenUpdateThreadProc, this, kRealtimePriority);
    _screenUpdateEvent = EventWrapper::Create();

    GetWindowRect(_windowRect);

    _lastViewBounds.origin.x = 0;
    _lastViewBounds.origin.y = 0;
    _lastViewBounds.size.width = 0;
    _lastViewBounds.size.height = 0;

#ifdef NEW_HIVIEW_PARENT_EVENT_HANDLER
    // This gets the parent window of the HIViewRef that's passed in and installs a WindowRef event handler on it
    // The event handler looks for window resize events and adjusts the offset of the controls.

    //WEBRTC_TRACE(kTraceDebug, "%s:%d Installing Eventhandler for hiviewRef's parent window", __FUNCTION__, __LINE__);


    static const EventTypeSpec windowEventTypes[] =
    {
        kEventClassWindow, kEventWindowBoundsChanged,
        kEventClassWindow, kEventWindowBoundsChanging,
        kEventClassWindow, kEventWindowZoomed,
        kEventClassWindow, kEventWindowExpanded,
        kEventClassWindow, kEventWindowClickResizeRgn,
        kEventClassWindow, kEventWindowClickDragRgn
    };

    WindowRef parentWindow = HIViewGetWindow(windowRef);

    InstallWindowEventHandler (parentWindow,
            NewEventHandlerUPP (sHandleWindowResized),
            GetEventTypeCount(windowEventTypes),
            windowEventTypes,
            (void *) this, // this is an arbitrary parameter that will be passed on to your event handler when it is called later
            &_windowEventHandlerRef);

#endif

#ifdef NEW_HIVIEW_EVENT_HANDLER	
    //WEBRTC_TRACE(kTraceDebug, "%s:%d Installing Eventhandler for hiviewRef", __FUNCTION__, __LINE__);

    static const EventTypeSpec hiviewEventTypes[] =
    {
        kEventClassControl, kEventControlBoundsChanged,
        kEventClassControl, kEventControlDraw
        //			kEventControlDragLeave
        //			kEventControlDragReceive
        //			kEventControlGetFocusPart
        //			kEventControlApplyBackground
        //			kEventControlDraw
        //			kEventControlHit

    };

    HIViewInstallEventHandler(_hiviewRef,
            NewEventHandlerUPP(sHandleHiViewResized),
            GetEventTypeCount(hiviewEventTypes),
            hiviewEventTypes,
            (void *) this,
            &_hiviewEventHandlerRef);

#endif
}

// this is a static function. It has been registered (in constructor) to be called on various window redrawing or resizing.
// Since it is a static method, I have passed in "this" as the userData (one and only allowed) parameter, then calling member methods on it.
#pragma mark HIViewRef Event Handler
pascal OSStatus VideoRenderAGL::sHandleHiViewResized (EventHandlerCallRef nextHandler, EventRef theEvent, void* userData)
{
    //static int      callbackCounter = 1;
    HIViewRef hiviewRef = NULL;

    // see https://dcs.sourcerepo.com/dcs/tox_view/trunk/tox/libraries/i686-win32/include/quicktime/CarbonEvents.h for a list of codes
    int eventType = GetEventKind(theEvent);
    OSStatus status = noErr;
    status = GetEventParameter (theEvent,
            kEventParamDirectObject,
            typeControlRef,
            NULL,
            sizeof (ControlRef),
            NULL,
            &hiviewRef);

    VideoRenderAGL* obj = (VideoRenderAGL*)(userData);
    WindowRef parentWindow = HIViewGetWindow(hiviewRef);
    bool updateUI = true;

    if(kEventControlBoundsChanged == eventType)
    {
    }
    else if(kEventControlDraw == eventType)
    {
    }
    else
    {
        updateUI = false;
    }

    if(true == updateUI)
    {
        obj->ParentWindowResized(parentWindow);
        obj->UpdateClipping();
        obj->RenderOffScreenBuffers();
    }

    return status;
}

VideoRenderAGL::~VideoRenderAGL()
{

    //WEBRTC_TRACE(kTraceDebug, "%s:%d Destructor", __FUNCTION__, __LINE__);


#ifdef USE_EVENT_HANDLERS
    // remove event handlers
    OSStatus status;
    if(_isHIViewRef)
    {
        status = RemoveEventHandler(_hiviewEventHandlerRef);
    }
    else
    {
        status = RemoveEventHandler(_windowEventHandlerRef);
    }
    if(noErr != status)
    {
        if(_isHIViewRef)
        {

            //WEBRTC_TRACE(kTraceDebug, "%s:%d Failed to remove hiview event handler: %d", __FUNCTION__, __LINE__, (int)_hiviewEventHandlerRef);
        }
        else
        {
            //WEBRTC_TRACE(kTraceDebug, "%s:%d Failed to remove window event handler %d", __FUNCTION__, __LINE__, (int)_windowEventHandlerRef);
        }
    }

#endif

    OSStatus status;
#ifdef NEW_HIVIEW_PARENT_EVENT_HANDLER
    if(_windowEventHandlerRef)
    {
        status = RemoveEventHandler(_windowEventHandlerRef);
        if(status != noErr)
        {
            //WEBRTC_TRACE(kTraceDebug, "%s:%d failed to remove window event handler %d", __FUNCTION__, __LINE__, (int)_windowEventHandlerRef);
        }
    }
#endif

#ifdef NEW_HIVIEW_EVENT_HANDLER	
    if(_hiviewEventHandlerRef)
    {
        status = RemoveEventHandler(_hiviewEventHandlerRef);
        if(status != noErr)
        {
            //WEBRTC_TRACE(kTraceDebug, "%s:%d Failed to remove hiview event handler: %d", __FUNCTION__, __LINE__, (int)_hiviewEventHandlerRef);
        }
    }
#endif

    // Signal event to exit thread, then delete it
    ThreadWrapper* tmpPtr = _screenUpdateThread;
    _screenUpdateThread = NULL;

    if (tmpPtr)
    {
        tmpPtr->SetNotAlive();
        _screenUpdateEvent->Set();
        _screenUpdateEvent->StopTimer();

        if (tmpPtr->Stop())
        {
            delete tmpPtr;
        }
        delete _screenUpdateEvent;
        _screenUpdateEvent = NULL;
    }

    if (_aglContext != 0)
    {
        aglSetCurrentContext(_aglContext);
        aglDestroyContext(_aglContext);
        _aglContext = 0;
    }

    // Delete all channels
    std::map<int, VideoChannelAGL*>::iterator it = _aglChannels.begin();
    while (it!= _aglChannels.end())
    {
        delete it->second;
        _aglChannels.erase(it);
        it = _aglChannels.begin();
    }
    _aglChannels.clear();

    // Clean the zOrder map
    std::multimap<int, int>::iterator zIt = _zOrderToChannel.begin();
    while(zIt != _zOrderToChannel.end())
    {
        _zOrderToChannel.erase(zIt);
        zIt = _zOrderToChannel.begin();
    }
    _zOrderToChannel.clear();

    //delete _renderCritSec;


}

int VideoRenderAGL::GetOpenGLVersion(int& aglMajor, int& aglMinor)
{
    aglGetVersion((GLint *) &aglMajor, (GLint *) &aglMinor);
    return 0;
}

int VideoRenderAGL::Init()
{
    LockAGLCntx();

    // Start rendering thread...
    if (!_screenUpdateThread)
    {
        UnlockAGLCntx();
        //WEBRTC_TRACE(kTraceError, "%s:%d Thread not created", __FUNCTION__, __LINE__);
        return -1;
    }
    unsigned int threadId;
    _screenUpdateThread->Start(threadId);

    // Start the event triggering the render process
    unsigned int monitorFreq = 60;
    _screenUpdateEvent->StartTimer(true, 1000/monitorFreq);

    // Create mixing textures
    if (CreateMixingContext() == -1)
    {
        //WEBRTC_TRACE(kTraceError, "%s:%d Could not create a mixing context", __FUNCTION__, __LINE__);
        UnlockAGLCntx();
        return -1;
    }

    UnlockAGLCntx();
    return 0;
}

VideoChannelAGL* VideoRenderAGL::CreateAGLChannel(int channel, int zOrder, float startWidth, float startHeight, float stopWidth, float stopHeight)
{

    LockAGLCntx();

    //WEBRTC_TRACE(kTraceInfo, "%s:%d Creating AGL channel: %d", __FUNCTION__, __LINE__, channel);

    if (HasChannel(channel))
    {
        //WEBRTC_TRACE(kTraceError, "%s:%d Channel already exists", __FUNCTION__, __LINE__);
        UnlockAGLCntx();k
        return NULL;
    }

    if (_zOrderToChannel.find(zOrder) != _zOrderToChannel.end())
    {
        // There are already one channel using this zOrder
        // TODO: Allow multiple channels with same zOrder
    }

    VideoChannelAGL* newAGLChannel = new VideoChannelAGL(_aglContext, _id, this);

    if (newAGLChannel->SetStreamSettings(0, startWidth, startHeight, stopWidth, stopHeight) == -1)
    {
        if (newAGLChannel)
        {
            delete newAGLChannel;
            newAGLChannel = NULL;
        }
        //WEBRTC_LOG(kTraceError, "Could not create AGL channel");
        //WEBRTC_TRACE(kTraceError, "%s:%d Could not create AGL channel", __FUNCTION__, __LINE__);
        UnlockAGLCntx();
        return NULL;
    }
k
    _aglChannels[channel] = newAGLChannel;
    _zOrderToChannel.insert(std::pair<int, int>(zOrder, channel));

    UnlockAGLCntx();
    return newAGLChannel;
}

int VideoRenderAGL::DeleteAllAGLChannels()
{
    CriticalSectionScoped cs(&_renderCritSec);

    //WEBRTC_TRACE(kTraceInfo, "%s:%d Deleting all AGL channels", __FUNCTION__, __LINE__);
    //int i = 0 ;
    std::map<int, VideoChannelAGL*>::iterator it;
    it = _aglChannels.begin();

    while (it != _aglChannels.end())
    {
        VideoChannelAGL* channel = it->second;
        if (channel)
        delete channel;

        _aglChannels.erase(it);
        it = _aglChannels.begin();
    }
    _aglChannels.clear();
    return 0;
}

int VideoRenderAGL::DeleteAGLChannel(int channel)
{
    CriticalSectionScoped cs(&_renderCritSec);
    //WEBRTC_TRACE(kTraceDebug, "%s:%d Deleting AGL channel %d", __FUNCTION__, __LINE__, channel);

    std::map<int, VideoChannelAGL*>::iterator it;
    it = _aglChannels.find(channel);
    if (it != _aglChannels.end())
    {
        delete it->second;
        _aglChannels.erase(it);
    }
    else
    {
        //WEBRTC_TRACE(kTraceWarning, "%s:%d Channel not found", __FUNCTION__, __LINE__);
        return -1;
    }

    std::multimap<int, int>::iterator zIt = _zOrderToChannel.begin();
    while( zIt != _zOrderToChannel.end())
    {
        if (zIt->second == channel)
        {
            _zOrderToChannel.erase(zIt);
            break;
        }
        zIt++;// = _zOrderToChannel.begin();
    }

    return 0;
}

int VideoRenderAGL::StopThread()
{
    CriticalSectionScoped cs(&_renderCritSec);
    ThreadWrapper* tmpPtr = _screenUpdateThread;
    //_screenUpdateThread = NULL;

    if (tmpPtr)
    {
        tmpPtr->SetNotAlive();
        _screenUpdateEvent->Set();
        if (tmpPtr->Stop())
        {
            delete tmpPtr;
        }
    }

    delete _screenUpdateEvent;
    _screenUpdateEvent = NULL;

    return 0;
}

bool VideoRenderAGL::IsFullScreen()
{
    CriticalSectionScoped cs(&_renderCritSec);
    return _fullScreen;
}

bool VideoRenderAGL::HasChannels()
{

    CriticalSectionScoped cs(&_renderCritSec);

    if (_aglChannels.begin() != _aglChannels.end())
    {
        return true;
    }

    return false;
}

bool VideoRenderAGL::HasChannel(int channel)
{
    CriticalSectionScoped cs(&_renderCritSec);

    std::map<int, VideoChannelAGL*>::iterator it = _aglChannels.find(channel);
    if (it != _aglChannels.end())
    {
        return true;
    }

    return false;
}

int VideoRenderAGL::GetChannels(std::list<int>& channelList)
{

    CriticalSectionScoped cs(&_renderCritSec);
    std::map<int, VideoChannelAGL*>::iterator it = _aglChannels.begin();

    while (it != _aglChannels.end())
    {
        channelList.push_back(it->first);
        it++;
    }

    return 0;
}

VideoChannelAGL* VideoRenderAGL::ConfigureAGLChannel(int channel, int zOrder, float startWidth, float startHeight, float stopWidth, float stopHeight)
{

    CriticalSectionScoped cs(&_renderCritSec);

    std::map<int, VideoChannelAGL*>::iterator it = _aglChannels.find(channel);

    if (it != _aglChannels.end())
    {
        VideoChannelAGL* aglChannel = it->second;
        if (aglChannel->SetStreamSettings(0, startWidth, startHeight, stopWidth, stopHeight) == -1)
        {
            return NULL;
        }

        std::multimap<int, int>::iterator it = _zOrderToChannel.begin();
        while(it != _zOrderToChannel.end())
        {
            if (it->second == channel)
            {
                if (it->first != zOrder)
                {
                    _zOrderToChannel.erase(it);
                    _zOrderToChannel.insert(std::pair<int, int>(zOrder, channel));
                }
                break;
            }
            it++;
        }
        return aglChannel;
    }

    return NULL;
}

bool VideoRenderAGL::ScreenUpdateThreadProc(void* obj)
{
    return static_cast<VideoRenderAGL*>(obj)->ScreenUpdateProcess();
}

bool VideoRenderAGL::ScreenUpdateProcess()
{
    _screenUpdateEvent->Wait(100);

    LockAGLCntx();

    if (!_screenUpdateThread)
    {
        UnlockAGLCntx();
        return false;
    }

    if (aglSetCurrentContext(_aglContext) == GL_FALSE)
    {
        UnlockAGLCntx();
        return true;
    }

    if (GetWindowRect(_windowRect) == -1)
    {
        UnlockAGLCntx();
        return true;
    }

    if (_windowWidth != (_windowRect.right - _windowRect.left)
            || _windowHeight != (_windowRect.bottom - _windowRect.top))
    {
        // We have a new window size, update the context.
        if (aglUpdateContext(_aglContext) == GL_FALSE)
        {
            UnlockAGLCntx();
            return true;
        }
        _windowWidth = _windowRect.right - _windowRect.left;
        _windowHeight = _windowRect.bottom - _windowRect.top;
    }

    // this section will poll to see if the window size has changed
    // this is causing problem w/invalid windowRef
    // this code has been modified and exists now in the window event handler
#ifndef NEW_HIVIEW_PARENT_EVENT_HANDLER
    if (_isHIViewRef)
    {

        if(FALSE == HIViewIsValid(_hiviewRef))
        {

            //WEBRTC_TRACE(kTraceDebug, "%s:%d Invalid windowRef", __FUNCTION__, __LINE__);
            UnlockAGLCntx();
            return true;
        }
        WindowRef window = HIViewGetWindow(_hiviewRef);

        if(FALSE == IsValidWindowPtr(window))
        {
            //WEBRTC_TRACE(kTraceDebug, "%s:%d Invalide hiviewRef", __FUNCTION__, __LINE__);
            UnlockAGLCntx();
            return true;
        }
        if (window == NULL)
        {
            //WEBRTC_TRACE(kTraceDebug, "%s:%d WindowRef = NULL", __FUNCTION__, __LINE__);
            UnlockAGLCntx();
            return true;
        }

        if(FALSE == MacIsWindowVisible(window))
        {
            //WEBRTC_TRACE(kTraceDebug, "%s:%d MacIsWindowVisible == FALSE. Returning early", __FUNCTION__, __LINE__);
            UnlockAGLCntx();
            return true;
        }

        HIRect viewBounds; // Placement and size for HIView
        int windowWidth = 0; // Parent window width
        int windowHeight = 0; // Parent window height

        // NOTE: Calling GetWindowBounds with kWindowStructureRgn will crash intermittentaly if the OS decides it needs to push it into the back for a moment.
        // To counter this, we get the titlebar height on class construction and then add it to the content region here. Content regions seems not to crash
        Rect contentBounds =
        {   0, 0, 0, 0}; // The bounds for the parent window

#if		defined(USE_CONTENT_RGN)
        GetWindowBounds(window, kWindowContentRgn, &contentBounds);
#elif	defined(USE_STRUCT_RGN)
        GetWindowBounds(window, kWindowStructureRgn, &contentBounds);
#endif

        Rect globalBounds =
        {   0, 0, 0, 0}; // The bounds for the parent window
        globalBounds.top = contentBounds.top;
        globalBounds.right = contentBounds.right;
        globalBounds.bottom = contentBounds.bottom;
        globalBounds.left = contentBounds.left;

        windowHeight = globalBounds.bottom - globalBounds.top;
        windowWidth = globalBounds.right - globalBounds.left;

        // Get the size of the HIViewRef
        HIViewGetBounds(_hiviewRef, &viewBounds);
        HIViewConvertRect(&viewBounds, _hiviewRef, NULL);

        // Check if this is the first call..
        if (_lastWindowHeight == -1 &&
                _lastWindowWidth == -1)
        {
            _lastWindowWidth = windowWidth;
            _lastWindowHeight = windowHeight;

            _lastViewBounds.origin.x = viewBounds.origin.x;
            _lastViewBounds.origin.y = viewBounds.origin.y;
            _lastViewBounds.size.width = viewBounds.size.width;
            _lastViewBounds.size.height = viewBounds.size.height;
        }
        sfasdfasdf

        bool resized = false;

        // Check if parent window size has changed
        if (windowHeight != _lastWindowHeight ||
                windowWidth != _lastWindowWidth)
        {
            resized = true;
        }

        // Check if the HIView has new size or is moved in the parent window
        if (_lastViewBounds.origin.x != viewBounds.origin.x ||
                _lastViewBounds.origin.y != viewBounds.origin.y ||
                _lastViewBounds.size.width != viewBounds.size.width ||
                _lastViewBounds.size.height != viewBounds.size.height)
        {
            // The HiView is resized or has moved.
            resized = true;
        }

        if (resized)
        {

            //WEBRTC_TRACE(kTraceDebug, "%s:%d Window has resized", __FUNCTION__, __LINE__);

            // Calculate offset between the windows
            // {x, y, widht, height}, x,y = lower left corner
            const GLint offs[4] =
            {   (int)(0.5f + viewBounds.origin.x),
                (int)(0.5f + windowHeight - (viewBounds.origin.y + viewBounds.size.height)),
                viewBounds.size.width, viewBounds.size.height};

            //WEBRTC_TRACE(kTraceDebug, "%s:%d contentBounds	t:%d r:%d b:%d l:%d", __FUNCTION__, __LINE__,
            contentBounds.top, contentBounds.right, contentBounds.bottom, contentBounds.left);
            //WEBRTC_TRACE(kTraceDebug, "%s:%d windowHeight=%d", __FUNCTION__, __LINE__, windowHeight);
            //WEBRTC_TRACE(kTraceDebug, "%s:%d offs[4] = %d, %d, %d, %d", __FUNCTION__, __LINE__, offs[0], offs[1], offs[2], offs[3]);

            aglSetDrawable (_aglContext, GetWindowPort(window));
            aglSetInteger(_aglContext, AGL_BUFFER_RECT, offs);
            aglEnable(_aglContext, AGL_BUFFER_RECT);

            // We need to change the viewport too if the HIView size has changed
            glViewport(0.0f, 0.0f, (GLsizei) viewBounds.size.width, (GLsizei) viewBounds.size.height);

        }
        _lastWindowWidth = windowWidth;
        _lastWindowHeight = windowHeight;

        _lastViewBounds.origin.x = viewBounds.origin.x;
        _lastViewBounds.origin.y = viewBounds.origin.y;
        _lastViewBounds.size.width = viewBounds.size.width;
        _lastViewBounds.size.height = viewBounds.size.height;

    }
#endif
    if (_fullScreen)
    {
        // TODO
        // We use double buffers, must always update
        //RenderOffScreenBuffersToBackBuffer();
    }
    else
    {
        // Check if there are any updated buffers
        bool updated = false;

        // TODO: check if window size is updated!
        // TODO Improvement: Walk through the zOrder Map to only render the ones in need of update
        std::map<int, VideoChannelAGL*>::iterator it = _aglChannels.begin();
        while (it != _aglChannels.end())
        {

            VideoChannelAGL* aglChannel = it->second;
            aglChannel->UpdateStretchSize(_windowHeight, _windowWidth);
            aglChannel->IsUpdated(updated);
            if (updated)
            {
                break;
            }
            it++;
        }

        if (updated)
        {
            // At least on buffers is updated, we need to repaint the texture
            if (RenderOffScreenBuffers() != -1)
            {
                // MF
                //SwapAndDisplayBuffers();
            }
            else
            {
                // Error updating the mixing texture, don't swap.
            }
        }
    }

    UnlockAGLCntx();

    //WEBRTC_LOG(kTraceDebug, "Leaving ScreenUpdateProcess()");
    return true;
}

void VideoRenderAGL::ParentWindowResized(WindowRef window)
{
    //WEBRTC_LOG(kTraceDebug, "%s HIViewRef:%d owner window has resized", __FUNCTION__, (int)_hiviewRef);

    LockAGLCntx();
k
    // set flag
    _windowHasResized = false;

    if(FALSE == HIViewIsValid(_hiviewRef))
    {
        //WEBRTC_LOG(kTraceDebug, "invalid windowRef");
        UnlockAGLCntx();
        return;
    }

    if(FALSE == IsValidWindowPtr(window))
    {
        //WEBRTC_LOG(kTraceError, "invalid windowRef");
        UnlockAGLCntx();
        return;
    }

    if (window == NULL)
    {
        //WEBRTC_LOG(kTraceError, "windowRef = NULL");
        UnlockAGLCntx();
        return;
    }

    if(FALSE == MacIsWindowVisible(window))
    {
        //WEBRTC_LOG(kTraceDebug, "MacIsWindowVisible = FALSE. Returning early.");
        UnlockAGLCntx();
        return;
    }

    Rect contentBounds =
    {   0, 0, 0, 0};

#if		defined(USE_CONTENT_RGN)
    GetWindowBounds(window, kWindowContentRgn, &contentBounds);
#elif	defined(USE_STRUCT_RGN)
    GetWindowBounds(window, kWindowStructureRgn, &contentBounds);
#endif

    //WEBRTC_LOG(kTraceDebug, "%s contentBounds	t:%d r:%d b:%d l:%d", __FUNCTION__, contentBounds.top, contentBounds.right, contentBounds.bottom, contentBounds.left);

    // update global vars
    _currentParentWindowBounds.top = contentBounds.top;
    _currentParentWindowBounds.left = contentBounds.left;
    _currentParentWindowBounds.bottom = contentBounds.bottom;
    _currentParentWindowBounds.right = contentBounds.right;

    _currentParentWindowWidth = _currentParentWindowBounds.right - _currentParentWindowBounds.left;
    _currentParentWindowHeight = _currentParentWindowBounds.bottom - _currentParentWindowBounds.top;

    _windowHasResized = true;

    // ********* update AGL offsets
    HIRect viewBounds;
    HIViewGetBounds(_hiviewRef, &viewBounds);
    HIViewConvertRect(&viewBounds, _hiviewRef, NULL);

    const GLint offs[4] =
    {   (int)(0.5f + viewBounds.origin.x),
        (int)(0.5f + _currentParentWindowHeight - (viewBounds.origin.y + viewBounds.size.height)),
        viewBounds.size.width, viewBounds.size.height};
    //WEBRTC_LOG(kTraceDebug, "%s _currentParentWindowHeight=%d", __FUNCTION__, _currentParentWindowHeight);
    //WEBRTC_LOG(kTraceDebug, "%s offs[4] = %d, %d, %d, %d", __FUNCTION__, offs[0], offs[1], offs[2], offs[3]);

    aglSetCurrentContext(_aglContext);
    aglSetDrawable (_aglContext, GetWindowPort(window));
    aglSetInteger(_aglContext, AGL_BUFFER_RECT, offs);
    aglEnable(_aglContext, AGL_BUFFER_RECT);

    // We need to change the viewport too if the HIView size has changed
    glViewport(0.0f, 0.0f, (GLsizei) viewBounds.size.width, (GLsizei) viewBounds.size.height);

    UnlockAGLCntx();

    return;
}

int VideoRenderAGL::CreateMixingContext()
{

    LockAGLCntx();

    //WEBRTC_LOG(kTraceDebug, "Entering CreateMixingContext()");

    // Use both AGL_ACCELERATED and AGL_NO_RECOVERY to make sure 
    // a hardware renderer is used and not a software renderer.

    GLint attributes[] =
    {
        AGL_DOUBLEBUFFER,
        AGL_WINDOW,
        AGL_RGBA,
        AGL_NO_RECOVERY,
        AGL_ACCELERATED,
        AGL_RED_SIZE, 8,
        AGL_GREEN_SIZE, 8,
        AGL_BLUE_SIZE, 8,
        AGL_ALPHA_SIZE, 8,
        AGL_DEPTH_SIZE, 24,
        AGL_NONE,
    };

    AGLPixelFormat aglPixelFormat;

    // ***** Set up the OpenGL Context *****

    // Get a pixel format for the attributes above
    aglPixelFormat = aglChoosePixelFormat(NULL, 0, attributes);
    if (NULL == aglPixelFormat)
    {
        //WEBRTC_LOG(kTraceError, "Could not create pixel format");
        UnlockAGLCntx();
        return -1;
    }

    // Create an AGL context
    _aglContext = aglCreateContext(aglPixelFormat, NULL);
    if (_aglContext == NULL)
    {
        //WEBRTC_LOG(kTraceError, "Could no create AGL context");
        UnlockAGLCntx();
        return -1;
    }

    // Release the pixel format memory
    aglDestroyPixelFormat(aglPixelFormat);

    // Set the current AGL context for the rest of the settings
    if (aglSetCurrentContext(_aglContext) == false)
    {
        //WEBRTC_LOG(kTraceError, "Could not set current context: %d", aglGetError());
        UnlockAGLCntx();
        return -1;
    }

    if (_isHIViewRef)
    {
        //---------------------------
        // BEGIN: new test code
#if 0
        // Don't use this one!
        // There seems to be an OS X bug that can't handle
        // movements and resizing of the parent window
        // and or the HIView
        if (aglSetHIViewRef(_aglContext,_hiviewRef) == false)
        {
            //WEBRTC_LOG(kTraceError, "Could not set WindowRef: %d", aglGetError());
            UnlockAGLCntx();
            return -1;
        }
#else

        // Get the parent window for this control
        WindowRef window = GetControlOwner(_hiviewRef);

        Rect globalBounds =
        {   0,0,0,0}; // The bounds for the parent window
        HIRect viewBounds; // Placemnt in the parent window and size.
        int windowHeight = 0;

        //		Rect titleBounds = {0,0,0,0};
        //		GetWindowBounds(window, kWindowTitleBarRgn, &titleBounds);
        //		_titleBarHeight = titleBounds.top - titleBounds.bottom;
        //		if(0 == _titleBarHeight)
        //		{
        //            //WEBRTC_LOG(kTraceError, "Titlebar height = 0");
        //            //return -1;
        //		}


        // Get the bounds for the parent window
#if		defined(USE_CONTENT_RGN)
        GetWindowBounds(window, kWindowContentRgn, &globalBounds);
#elif	defined(USE_STRUCT_RGN)
        GetWindowBounds(window, kWindowStructureRgn, &globalBounds);
#endif
        windowHeight = globalBounds.bottom - globalBounds.top;

        // Get the bounds for the HIView
        HIViewGetBounds(_hiviewRef, &viewBounds);

        HIViewConvertRect(&viewBounds, _hiviewRef, NULL);

        const GLint offs[4] =
        {   (int)(0.5f + viewBounds.origin.x),
            (int)(0.5f + windowHeight - (viewBounds.origin.y + viewBounds.size.height)),
            viewBounds.size.width, viewBounds.size.height};

        //WEBRTC_LOG(kTraceDebug, "%s offs[4] = %d, %d, %d, %d", __FUNCTION__, offs[0], offs[1], offs[2], offs[3]);


        aglSetDrawable (_aglContext, GetWindowPort(window));
        aglSetInteger(_aglContext, AGL_BUFFER_RECT, offs);
        aglEnable(_aglContext, AGL_BUFFER_RECT);

        GLint surfaceOrder = 1; // 1: above window, -1 below.
        //OSStatus status = aglSetInteger(_aglContext, AGL_SURFACE_ORDER, &surfaceOrder);
        aglSetInteger(_aglContext, AGL_SURFACE_ORDER, &surfaceOrder);

        glViewport(0.0f, 0.0f, (GLsizei) viewBounds.size.width, (GLsizei) viewBounds.size.height);
#endif

    }
    else
    {
        if(GL_FALSE == aglSetDrawable (_aglContext, GetWindowPort(_windowRef)))
        {
            //WEBRTC_LOG(kTraceError, "Could not set WindowRef: %d", aglGetError());
            UnlockAGLCntx();
            return -1;
        }
    }

    _windowWidth = _windowRect.right - _windowRect.left;
    _windowHeight = _windowRect.bottom - _windowRect.top;

    // opaque surface
    int surfaceOpacity = 1;
    if (aglSetInteger(_aglContext, AGL_SURFACE_OPACITY, (const GLint *) &surfaceOpacity) == false)
    {
        //WEBRTC_LOG(kTraceError, "Could not set surface opacity: %d", aglGetError());
        UnlockAGLCntx();
        return -1;
    }

    // 1 -> sync to screen rat, slow...
    //int swapInterval = 0;  // 0 don't sync with vertical trace
    int swapInterval = 0; // 1 sync with vertical trace
    if (aglSetInteger(_aglContext, AGL_SWAP_INTERVAL, (const GLint *) &swapInterval) == false)
    {
        //WEBRTC_LOG(kTraceError, "Could not set swap interval: %d", aglGetError());
        UnlockAGLCntx();
        return -1;
    }

    // Update the rect with the current size
    if (GetWindowRect(_windowRect) == -1)
    {
        //WEBRTC_LOG(kTraceError, "Could not get window size");
        UnlockAGLCntx();
        return -1;
    }

    // Disable not needed functionality to increase performance
    glDisable(GL_DITHER);
    glDisable(GL_ALPHA_TEST);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_FOG);
    glDisable(GL_TEXTURE_2D);
    glPixelZoom(1.0, 1.0);

    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    GLenum glErr = glGetError();

    if (glErr)
    {
    }

    UpdateClipping();

    //WEBRTC_LOG(kTraceDebug, "Leaving CreateMixingContext()");

    UnlockAGLCntx();
    return 0;
}

int VideoRenderAGL::RenderOffScreenBuffers()
{
    LockAGLCntx();

    // Get the current window size, it might have changed since last render.
    if (GetWindowRect(_windowRect) == -1)
    {
        //WEBRTC_LOG(kTraceError, "Could not get window rect");
        UnlockAGLCntx();
        return -1;
    }

    if (aglSetCurrentContext(_aglContext) == false)
    {
        //WEBRTC_LOG(kTraceError, "Could not set current context for rendering");
        UnlockAGLCntx();
        return -1;
    }

    // HERE - onl if updated!
    glClear(GL_COLOR_BUFFER_BIT);

    // Loop through all channels starting highest zOrder ending with lowest.
    for (std::multimap<int, int>::reverse_iterator rIt = _zOrderToChannel.rbegin();
    rIt != _zOrderToChannel.rend();
    rIt++)
    {
        int channelId = rIt->second;
        std::map<int, VideoChannelAGL*>::iterator it = _aglChannels.find(channelId);

        VideoChannelAGL* aglChannel = it->second;

        aglChannel->RenderOffScreenBuffer();
    }

    SwapAndDisplayBuffers();

    UnlockAGLCntx();
    return 0;
}

int VideoRenderAGL::SwapAndDisplayBuffers()
{

    LockAGLCntx();
    if (_fullScreen)
    {
        // TODO:
        // Swap front and back buffers, rendering taking care of in the same call
        //aglSwapBuffers(_aglContext);
        // Update buffer index to the idx for the next rendering!
        //_textureIdx = (_textureIdx + 1) & 1;
    }
    else
    {
        // Single buffer rendering, only update context.
        glFlush();
        aglSwapBuffers(_aglContext);
        HIViewSetNeedsDisplay(_hiviewRef, true);
    }

    UnlockAGLCntx();
    return 0;
}

int VideoRenderAGL::GetWindowRect(Rect& rect)
{

    LockAGLCntx();

    if (_isHIViewRef)
    {
        if (_hiviewRef)
        {
            HIRect HIViewRect1;
            if(FALSE == HIViewIsValid(_hiviewRef))
            {
                rect.top = 0;
                rect.left = 0;
                rect.right = 0;
                rect.bottom = 0;
                //WEBRTC_LOG(kTraceError,"GetWindowRect() HIViewIsValid() returned false");
                UnlockAGLCntx();
            }
            HIViewGetBounds(_hiviewRef,&HIViewRect1);
            HIRectConvert(&HIViewRect1, 1, NULL, 2, NULL);
            if(HIViewRect1.origin.x < 0)
            {
                rect.top = 0;
                //WEBRTC_LOG(kTraceDebug, "GetWindowRect() rect.top = 0");
            }
            else
            {
                rect.top = HIViewRect1.origin.x;
            }

            if(HIViewRect1.origin.y < 0)
            {
                rect.left = 0;
                //WEBRTC_LOG(kTraceDebug, "GetWindowRect() rect.left = 0");
            }
            else
            {
                rect.left = HIViewRect1.origin.y;
            }

            if(HIViewRect1.size.width < 0)
            {
                rect.right = 0;
                //WEBRTC_LOG(kTraceDebug, "GetWindowRect() rect.right = 0");
            }
            else
            {
                rect.right = HIViewRect1.size.width;
            }

            if(HIViewRect1.size.height < 0)
            {
                rect.bottom = 0;
                //WEBRTC_LOG(kTraceDebug, "GetWindowRect() rect.bottom = 0");
            }
            else
            {
                rect.bottom = HIViewRect1.size.height;
            }

            ////WEBRTC_LOG(kTraceDebug,"GetWindowRect() HIViewRef: rect.top = %d, rect.left = %d, rect.right = %d, rect.bottom =%d in GetWindowRect", rect.top,rect.left,rect.right,rect.bottom);
            UnlockAGLCntx();
        }
        else
        {
            //WEBRTC_LOG(kTraceError, "invalid HIViewRef");
            UnlockAGLCntx();
        }
    }
    else
    {
        if (_windowRef)
        {
            GetWindowBounds(_windowRef, kWindowContentRgn, &rect);
            UnlockAGLCntx();
        }
        else
        {
            //WEBRTC_LOG(kTraceError, "No WindowRef");
            UnlockAGLCntx();
        }
    }
}

int VideoRenderAGL::UpdateClipping()
{
    //WEBRTC_LOG(kTraceDebug, "Entering UpdateClipping()");
    LockAGLCntx();

    if(_isHIViewRef)
    {
        if(FALSE == HIViewIsValid(_hiviewRef))
        {
            //WEBRTC_LOG(kTraceError, "UpdateClipping() _isHIViewRef is invalid. Returning -1");
            UnlockAGLCntx();
            return -1;
        }

        RgnHandle visibleRgn = NewRgn();
        SetEmptyRgn (visibleRgn);

        if(-1 == CalculateVisibleRegion((ControlRef)_hiviewRef, visibleRgn, true))
        {
        }

        if(GL_FALSE == aglSetCurrentContext(_aglContext))
        {
            GLenum glErr = aglGetError();
            //WEBRTC_LOG(kTraceError, "aglSetCurrentContext returned FALSE with error code %d at line %d", glErr, __LINE__);
        }

        if(GL_FALSE == aglEnable(_aglContext, AGL_CLIP_REGION))
        {
            GLenum glErr = aglGetError();
            //WEBRTC_LOG(kTraceError, "aglEnable returned FALSE with error code %d at line %d\n", glErr, __LINE__);
        }

        if(GL_FALSE == aglSetInteger(_aglContext, AGL_CLIP_REGION, (const GLint*)visibleRgn))
        {
            GLenum glErr = aglGetError();
            //WEBRTC_LOG(kTraceError, "aglSetInteger returned FALSE with error code %d at line %d\n", glErr, __LINE__);
        }

        DisposeRgn(visibleRgn);
    }
    else
    {
        //WEBRTC_LOG(kTraceDebug, "Not using a hiviewref!\n");
    }

    //WEBRTC_LOG(kTraceDebug, "Leaving UpdateClipping()");
    UnlockAGLCntx();
    return true;
}

int VideoRenderAGL::CalculateVisibleRegion(ControlRef control, RgnHandle &visibleRgn, bool clipChildren)
{

    //	LockAGLCntx();

    //WEBRTC_LOG(kTraceDebug, "Entering CalculateVisibleRegion()");
    OSStatus osStatus = 0;
    OSErr osErr = 0;

    RgnHandle tempRgn = NewRgn();
    if (IsControlVisible(control))
    {
        RgnHandle childRgn = NewRgn();
        WindowRef window = GetControlOwner(control);
        ControlRef rootControl;
        GetRootControl(window, &rootControl); // 'wvnc'
        ControlRef masterControl;
        osStatus = GetSuperControl(rootControl, &masterControl);
        // //WEBRTC_LOG(kTraceDebug, "IBM GetSuperControl=%d", osStatus);

        if (masterControl != NULL)
        {
            CheckValidRegion(visibleRgn);
            // init visibleRgn with region of 'wvnc'
            osStatus = GetControlRegion(rootControl, kControlStructureMetaPart, visibleRgn);
            // //WEBRTC_LOG(kTraceDebug, "IBM GetControlRegion=%d : %d", osStatus, __LINE__);
            //GetSuperControl(rootControl, &rootControl);
            ControlRef tempControl = control, lastControl = 0;
            while (tempControl != masterControl) // current control != master

            {
                CheckValidRegion(tempRgn);

                // //WEBRTC_LOG(kTraceDebug, "IBM tempControl=%d masterControl=%d", tempControl, masterControl);
                ControlRef subControl;

                osStatus = GetControlRegion(tempControl, kControlStructureMetaPart, tempRgn); // intersect the region of the current control with visibleRgn
                // //WEBRTC_LOG(kTraceDebug, "IBM GetControlRegion=%d : %d", osStatus, __LINE__);
                CheckValidRegion(tempRgn);

                osErr = HIViewConvertRegion(tempRgn, tempControl, rootControl);
                // //WEBRTC_LOG(kTraceDebug, "IBM HIViewConvertRegion=%d : %d", osErr, __LINE__);
                CheckValidRegion(tempRgn);

                SectRgn(tempRgn, visibleRgn, visibleRgn);
                CheckValidRegion(tempRgn);
                CheckValidRegion(visibleRgn);
                if (EmptyRgn(visibleRgn)) // if the region is empty, bail
                break;

                if (clipChildren || tempControl != control) // clip children if true, cut out the tempControl if it's not one passed to this function

                {
                    UInt16 numChildren;
                    osStatus = CountSubControls(tempControl, &numChildren); // count the subcontrols
                    // //WEBRTC_LOG(kTraceDebug, "IBM CountSubControls=%d : %d", osStatus, __LINE__);

                    // //WEBRTC_LOG(kTraceDebug, "IBM numChildren=%d", numChildren);
                    for (int i = 0; i < numChildren; i++)
                    {
                        osErr = GetIndexedSubControl(tempControl, numChildren - i, &subControl); // retrieve the subcontrol in order by zorder
                        // //WEBRTC_LOG(kTraceDebug, "IBM GetIndexedSubControls=%d : %d", osErr, __LINE__);
                        if ( subControl == lastControl ) // break because of zorder

                        {
                            // //WEBRTC_LOG(kTraceDebug, "IBM breaking because of zorder %d", __LINE__);
                            break;
                        }

                        if (!IsControlVisible(subControl)) // dont' clip invisible controls

                        {
                            // //WEBRTC_LOG(kTraceDebug, "IBM continue. Control is not visible %d", __LINE__);
                            continue;
                        }

                        if(!subControl) continue;

                        osStatus = GetControlRegion(subControl, kControlStructureMetaPart, tempRgn); //get the region of the current control and union to childrg
                        // //WEBRTC_LOG(kTraceDebug, "IBM GetControlRegion=%d %d", osStatus, __LINE__);
                        CheckValidRegion(tempRgn);
                        if(osStatus != 0)
                        {
                            // //WEBRTC_LOG(kTraceDebug, "IBM ERROR! osStatus=%d. Continuing. %d", osStatus, __LINE__);
                            continue;
                        }
                        if(!tempRgn)
                        {
                            // //WEBRTC_LOG(kTraceDebug, "IBM ERROR! !tempRgn %d", osStatus, __LINE__);
                            continue;
                        }

                        osStatus = HIViewConvertRegion(tempRgn, subControl, rootControl);
                        CheckValidRegion(tempRgn);
                        // //WEBRTC_LOG(kTraceDebug, "IBM HIViewConvertRegion=%d %d", osStatus, __LINE__);
                        if(osStatus != 0)
                        {
                            // //WEBRTC_LOG(kTraceDebug, "IBM ERROR! osStatus=%d. Continuing. %d", osStatus, __LINE__);
                            continue;
                        }
                        if(!rootControl)
                        {
                            // //WEBRTC_LOG(kTraceDebug, "IBM ERROR! !rootControl %d", osStatus, __LINE__);
                            continue;
                        }

                        UnionRgn(tempRgn, childRgn, childRgn);
                        CheckValidRegion(tempRgn);
                        CheckValidRegion(childRgn);
                        CheckValidRegion(visibleRgn);
                        if(!childRgn)
                        {
                            // //WEBRTC_LOG(kTraceDebug, "IBM ERROR! !childRgn %d", osStatus, __LINE__);
                            continue;
                        }

                    } // next child control
                }
                lastControl = tempControl;
                GetSuperControl(tempControl, &subControl);
                tempControl = subControl;
            }

            DiffRgn(visibleRgn, childRgn, visibleRgn);
            CheckValidRegion(visibleRgn);
            CheckValidRegion(childRgn);
            DisposeRgn(childRgn);
        }
        else
        {
            CopyRgn(tempRgn, visibleRgn);
            CheckValidRegion(tempRgn);
            CheckValidRegion(visibleRgn);
        }
        DisposeRgn(tempRgn);
    }

    //WEBRTC_LOG(kTraceDebug, "Leaving CalculateVisibleRegion()");
    //_aglCritPtr->Leave();
    return 0;
}

bool VideoRenderAGL::CheckValidRegion(RgnHandle rHandle)
{

    Handle hndSize = (Handle)rHandle;
    long size = GetHandleSize(hndSize);
    if(0 == size)
    {

        OSErr memErr = MemError();
        if(noErr != memErr)
        {
            // //WEBRTC_LOG(kTraceError, "IBM ERROR Could not get size of handle. MemError() returned %d", memErr);
        }
        else
        {
            // //WEBRTC_LOG(kTraceError, "IBM ERROR Could not get size of handle yet MemError() returned noErr");
        }

    }
    else
    {
        // //WEBRTC_LOG(kTraceDebug, "IBM handleSize = %d", size);
    }

    if(false == IsValidRgnHandle(rHandle))
    {
        // //WEBRTC_LOG(kTraceError, "IBM ERROR Invalid Region found : $%d", rHandle);
        assert(false);
    }

    int err = QDError();
    switch(err)
    {
        case 0:
        break;
        case -147:
        //WEBRTC_LOG(kTraceError, "ERROR region too big");
        assert(false);
        break;

        case -149:
        //WEBRTC_LOG(kTraceError, "ERROR not enough stack");
        assert(false);
        break;

        default:
        //WEBRTC_LOG(kTraceError, "ERROR Unknown QDError %d", err);
        assert(false);
        break;
    }

    return true;
}

int VideoRenderAGL::ChangeWindow(void* newWindowRef)
{

    LockAGLCntx();

    UnlockAGLCntx();
    return -1;
}
WebRtc_Word32 VideoRenderAGL::ChangeUniqueID(WebRtc_Word32 id)
{
    LockAGLCntx();

    UnlockAGLCntx();
    return -1;
}

WebRtc_Word32 VideoRenderAGL::StartRender()
{

    LockAGLCntx();
    const unsigned int MONITOR_FREQ = 60;
    if(TRUE == _renderingIsPaused)
    {
        //WEBRTC_TRACE(kTraceInfo, kTraceVideoRenderer, _id, "%s:%d Rendering is paused. Restarting now", __FUNCTION__, __LINE__);

        // we already have the thread. Most likely StopRender() was called and they were paused
        if(FALSE == _screenUpdateThread->Start(_threadID))
        {
            //WEBRTC_TRACE(kTraceError, kTraceVideoRenderer, _id, "%s:%d Failed to start screenUpdateThread", __FUNCTION__, __LINE__);
            UnlockAGLCntx();
            return -1;
        }
        if(FALSE == _screenUpdateEvent->StartTimer(true, 1000/MONITOR_FREQ))
        {
            //WEBRTC_TRACE(kTraceError, kTraceVideoRenderer, _id, "%s:%d Failed to start screenUpdateEvent", __FUNCTION__, __LINE__);
            UnlockAGLCntx();
            return -1;
        }

        return 0;
    }

    _screenUpdateThread = ThreadWrapper::CreateThread(ScreenUpdateThreadProc, this, kRealtimePriority);
    _screenUpdateEvent = EventWrapper::Create();

    if (!_screenUpdateThread)
    {
        //WEBRTC_TRACE(kTraceError, kTraceVideoRenderer, _id, "%s:%d Failed to start screenUpdateThread", __FUNCTION__, __LINE__);
        UnlockAGLCntx();
        return -1;
    }

    _screenUpdateThread->Start(_threadID);
    _screenUpdateEvent->StartTimer(true, 1000/MONITOR_FREQ);

    //WEBRTC_TRACE(kTraceInfo, kTraceVideoRenderer, _id, "%s:%d Started screenUpdateThread", __FUNCTION__, __LINE__);

    UnlockAGLCntx();
    return 0;

}

WebRtc_Word32 VideoRenderAGL::StopRender()
{
    LockAGLCntx();

    if(!_screenUpdateThread || !_screenUpdateEvent)
    {
        _renderingIsPaused = TRUE;
        UnlockAGLCntx();
        return 0;
    }

    if(FALSE == _screenUpdateThread->Stop() || FALSE == _screenUpdateEvent->StopTimer())
    {
        _renderingIsPaused = FALSE;
        //WEBRTC_TRACE(kTraceInfo, kTraceVideoRenderer, _id, "%s:%d Could not stop either: screenUpdateThread or screenUpdateEvent", __FUNCTION__, __LINE__);
        UnlockAGLCntx();
        return -1;
    }

    _renderingIsPaused = TRUE;

    //WEBRTC_TRACE(kTraceInfo, kTraceVideoRenderer, _id, "%s:%d Stopped screenUpdateThread", __FUNCTION__, __LINE__);
    UnlockAGLCntx();
    return 0;
}

WebRtc_Word32 VideoRenderAGL::DeleteAGLChannel(const WebRtc_UWord32 streamID)
{

    LockAGLCntx();

    std::map<int, VideoChannelAGL*>::iterator it;
    it = _aglChannels.begin();

    while (it != _aglChannels.end())
    {
        VideoChannelAGL* channel = it->second;
        //WEBRTC_TRACE(kTraceInfo, kTraceVideoRenderer, _id, "%s:%d Deleting channel %d", __FUNCTION__, __LINE__, streamID);
        delete channel;
        it++;
    }
    _aglChannels.clear();

    UnlockAGLCntx();
    return 0;
}

WebRtc_Word32 VideoRenderAGL::GetChannelProperties(const WebRtc_UWord16 streamId,
WebRtc_UWord32& zOrder,
float& left,
float& top,
float& right,
float& bottom)
{

    LockAGLCntx();
    UnlockAGLCntx();
    return -1;

}

void VideoRenderAGL::LockAGLCntx()
{
    _renderCritSec.Enter();
}
void VideoRenderAGL::UnlockAGLCntx()
{
    _renderCritSec.Leave();
}

} //namespace webrtc

#endif   // CARBON_RENDERING

