/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_RENDER_MAIN_SOURCE_WINDOWS_VIDEO_RENDER_DIRECT3D9_H_
#define WEBRTC_MODULES_VIDEO_RENDER_MAIN_SOURCE_WINDOWS_VIDEO_RENDER_DIRECT3D9_H_

// WebRtc includes
#include "i_video_render_win.h"

#include <d3d9.h>
#include <d3dx9.h>
#include "ddraw.h"

#include <Map>

// Added
#include "video_render_defines.h"

#pragma comment(lib, "d3d9.lib")       // located in DirectX SDK

namespace webrtc {
class CriticalSectionWrapper;
class EventWrapper;
class Trace;
class ThreadWrapper;

class D3D9Channel: public VideoRenderCallback
{
public:
    D3D9Channel(LPDIRECT3DDEVICE9 pd3DDevice,
                    CriticalSectionWrapper* critSect, Trace* trace);

    virtual ~D3D9Channel();

    // Inherited from VideoRencerCallback, called from VideoAPI class.
    // Called when the incomming frame size and/or number of streams in mix changes
    virtual int FrameSizeChange(int width, int height, int numberOfStreams);

    // A new frame is delivered.
    virtual int DeliverFrame(const I420VideoFrame& videoFrame);
    virtual WebRtc_Word32 RenderFrame(const WebRtc_UWord32 streamId,
                                      I420VideoFrame& videoFrame);

    // Called to check if the video frame is updated.
    int IsUpdated(bool& isUpdated);
    // Called after the video frame has been render to the screen
    int RenderOffFrame();
    // Called to get the texture that contains the video frame
    LPDIRECT3DTEXTURE9 GetTexture();
    // Called to get the texture(video frame) size
    int GetTextureWidth();
    int GetTextureHeight();
    //
    void SetStreamSettings(WebRtc_UWord16 streamId,
                           WebRtc_UWord32 zOrder,
                           float startWidth,
                           float startHeight,
                           float stopWidth,
                           float stopHeight);
    int GetStreamSettings(WebRtc_UWord16 streamId,
                          WebRtc_UWord32& zOrder,
                          float& startWidth,
                          float& startHeight,
                          float& stopWidth,
                          float& stopHeight);

    int ReleaseTexture();
    int RecreateTexture(LPDIRECT3DDEVICE9 pd3DDevice);

protected:

private:
    //critical section passed from the owner
    CriticalSectionWrapper* _critSect;
    LPDIRECT3DDEVICE9 _pd3dDevice;
    LPDIRECT3DTEXTURE9 _pTexture;

    bool _bufferIsUpdated;
    // the frame size
    int _width;
    int _height;
    //sream settings
    //TODO support multiple streams in one channel
    WebRtc_UWord16 _streamId;
    WebRtc_UWord32 _zOrder;
    float _startWidth;
    float _startHeight;
    float _stopWidth;
    float _stopHeight;
};

class VideoRenderDirect3D9: IVideoRenderWin
{
public:
    VideoRenderDirect3D9(Trace* trace, HWND hWnd, bool fullScreen);
    ~VideoRenderDirect3D9();

public:
    //IVideoRenderWin

    /**************************************************************************
     *
     *   Init
     *
     ***************************************************************************/
    virtual WebRtc_Word32 Init();

    /**************************************************************************
     *
     *   Incoming Streams
     *
     ***************************************************************************/
    virtual VideoRenderCallback
            * CreateChannel(const WebRtc_UWord32 streamId,
                            const WebRtc_UWord32 zOrder,
                            const float left,
                            const float top,
                            const float right,
                            const float bottom);

    virtual WebRtc_Word32 DeleteChannel(const WebRtc_UWord32 streamId);

    virtual WebRtc_Word32 GetStreamSettings(const WebRtc_UWord32 channel,
                                            const WebRtc_UWord16 streamId,
                                            WebRtc_UWord32& zOrder,
                                            float& left,
                                            float& top,
                                            float& right,
                                            float& bottom);

    /**************************************************************************
     *
     *   Start/Stop
     *
     ***************************************************************************/

    virtual WebRtc_Word32 StartRender();
    virtual WebRtc_Word32 StopRender();

    /**************************************************************************
     *
     *   Properties
     *
     ***************************************************************************/

    virtual bool IsFullScreen();

    virtual WebRtc_Word32 SetCropping(const WebRtc_UWord32 channel,
                                      const WebRtc_UWord16 streamId,
                                      const float left,
                                      const float top,
                                      const float right,
                                      const float bottom);

    virtual WebRtc_Word32 ConfigureRenderer(const WebRtc_UWord32 channel,
                                            const WebRtc_UWord16 streamId,
                                            const unsigned int zOrder,
                                            const float left,
                                            const float top,
                                            const float right,
                                            const float bottom);

    virtual WebRtc_Word32 SetTransparentBackground(const bool enable);

    virtual WebRtc_Word32 ChangeWindow(void* window);

    virtual WebRtc_Word32 GetGraphicsMemory(WebRtc_UWord64& totalMemory,
                                            WebRtc_UWord64& availableMemory);

    virtual WebRtc_Word32 SetText(const WebRtc_UWord8 textId,
                                  const WebRtc_UWord8* text,
                                  const WebRtc_Word32 textLength,
                                  const WebRtc_UWord32 colorText,
                                  const WebRtc_UWord32 colorBg,
                                  const float left,
                                  const float top,
                                  const float rigth,
                                  const float bottom);

    virtual WebRtc_Word32 SetBitmap(const void* bitMap,
                                    const WebRtc_UWord8 pictureId,
                                    const void* colorKey,
                                    const float left,
                                    const float top,
                                    const float right,
                                    const float bottom);

public:
    // Get a channel by channel id
    D3D9Channel* GetD3DChannel(int channel);
    int UpdateRenderSurface();

protected:
    // The thread rendering the screen
    static bool ScreenUpdateThreadProc(void* obj);
    bool ScreenUpdateProcess();

private:
    // Init/close the d3d device
    int InitDevice();
    int CloseDevice();

    // Transparent related functions
    int SetTransparentColor(LPDIRECT3DTEXTURE9 pTexture,
                            DDCOLORKEY* transparentColorKey,
                            DWORD width,
                            DWORD height);

    CriticalSectionWrapper& _refD3DCritsect;
    Trace* _trace;
    ThreadWrapper* _screenUpdateThread;
    EventWrapper* _screenUpdateEvent;

    HWND _hWnd;
    bool _fullScreen;
    RECT _originalHwndRect;
    //FIXME we probably don't need this since all the information can be get from _d3dChannels
    int _channel;
    //Window size
    UINT _winWidth;
    UINT _winHeight;

    // Device
    LPDIRECT3D9 _pD3D; // Used to create the D3DDevice
    LPDIRECT3DDEVICE9 _pd3dDevice; // Our rendering device
    LPDIRECT3DVERTEXBUFFER9 _pVB; // Buffer to hold Vertices
    LPDIRECT3DTEXTURE9 _pTextureLogo;

    std::map<int, D3D9Channel*> _d3dChannels;
    std::multimap<int, unsigned int> _d3dZorder;

    // The position where the logo will be placed
    float _logoLeft;
    float _logoTop;
    float _logoRight;
    float _logoBottom;

    typedef HRESULT (WINAPI *DIRECT3DCREATE9EX)(UINT SDKVersion, IDirect3D9Ex**);
    LPDIRECT3DSURFACE9 _pd3dSurface;

    DWORD GetVertexProcessingCaps();
    int InitializeD3D(HWND hWnd, D3DPRESENT_PARAMETERS* pd3dpp);

    D3DPRESENT_PARAMETERS _d3dpp;
    int ResetDevice();

    int UpdateVerticeBuffer(LPDIRECT3DVERTEXBUFFER9 pVB, int offset,
                            float startWidth, float startHeight,
                            float stopWidth, float stopHeight);

    //code for providing graphics settings
    DWORD _totalMemory;
    DWORD _availableMemory;
};

} //namespace webrtc

#endif // WEBRTC_MODULES_VIDEO_RENDER_MAIN_SOURCE_WINDOWS_VIDEO_RENDER_DIRECT3D9_H_
