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
#include "webrtc/modules/video_render/windows/i_video_render_win.h"

#include <d3d9.h>
#include <d3dx9.h>
#include <ddraw.h>

#include <Map>

// Added
#include "webrtc/modules/video_render/include/video_render_defines.h"

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
    virtual int32_t RenderFrame(const uint32_t streamId,
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
    void SetStreamSettings(uint16_t streamId,
                           uint32_t zOrder,
                           float startWidth,
                           float startHeight,
                           float stopWidth,
                           float stopHeight);
    int GetStreamSettings(uint16_t streamId,
                          uint32_t& zOrder,
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
    uint16_t _streamId;
    uint32_t _zOrder;
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
    virtual int32_t Init();

    /**************************************************************************
     *
     *   Incoming Streams
     *
     ***************************************************************************/
    virtual VideoRenderCallback
            * CreateChannel(const uint32_t streamId,
                            const uint32_t zOrder,
                            const float left,
                            const float top,
                            const float right,
                            const float bottom);

    virtual int32_t DeleteChannel(const uint32_t streamId);

    virtual int32_t GetStreamSettings(const uint32_t channel,
                                      const uint16_t streamId,
                                      uint32_t& zOrder,
                                      float& left, float& top,
                                      float& right, float& bottom);

    /**************************************************************************
     *
     *   Start/Stop
     *
     ***************************************************************************/

    virtual int32_t StartRender();
    virtual int32_t StopRender();

    /**************************************************************************
     *
     *   Properties
     *
     ***************************************************************************/

    virtual bool IsFullScreen();

    virtual int32_t SetCropping(const uint32_t channel,
                                const uint16_t streamId,
                                const float left, const float top,
                                const float right, const float bottom);

    virtual int32_t ConfigureRenderer(const uint32_t channel,
                                      const uint16_t streamId,
                                      const unsigned int zOrder,
                                      const float left, const float top,
                                      const float right, const float bottom);

    virtual int32_t SetTransparentBackground(const bool enable);

    virtual int32_t ChangeWindow(void* window);

    virtual int32_t GetGraphicsMemory(uint64_t& totalMemory,
                                      uint64_t& availableMemory);

    virtual int32_t SetText(const uint8_t textId,
                            const uint8_t* text,
                            const int32_t textLength,
                            const uint32_t colorText,
                            const uint32_t colorBg,
                            const float left, const float top,
                            const float rigth, const float bottom);

    virtual int32_t SetBitmap(const void* bitMap,
                              const uint8_t pictureId,
                              const void* colorKey,
                              const float left, const float top,
                              const float right, const float bottom);

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

}  // namespace webrtc

#endif // WEBRTC_MODULES_VIDEO_RENDER_MAIN_SOURCE_WINDOWS_VIDEO_RENDER_DIRECT3D9_H_
