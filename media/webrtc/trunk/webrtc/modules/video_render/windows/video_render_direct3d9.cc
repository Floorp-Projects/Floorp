/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// Own include file
#include "webrtc/modules/video_render/windows/video_render_direct3d9.h"

// System include files
#include <windows.h>

// WebRtc include files
#include "webrtc/common_video/libyuv/include/webrtc_libyuv.h"
#include "webrtc/system_wrappers/include/critical_section_wrapper.h"
#include "webrtc/system_wrappers/include/event_wrapper.h"
#include "webrtc/system_wrappers/include/trace.h"

namespace webrtc {

// A structure for our custom vertex type
struct CUSTOMVERTEX
{
    FLOAT x, y, z;
    DWORD color; // The vertex color
    FLOAT u, v;
};

// Our custom FVF, which describes our custom vertex structure
#define D3DFVF_CUSTOMVERTEX (D3DFVF_XYZ|D3DFVF_DIFFUSE|D3DFVF_TEX1)

/*
 *
 *    D3D9Channel
 *
 */
D3D9Channel::D3D9Channel(LPDIRECT3DDEVICE9 pd3DDevice,
                                 CriticalSectionWrapper* critSect,
                                 Trace* trace) :
    _width(0),
    _height(0),
    _pd3dDevice(pd3DDevice),
    _pTexture(NULL),
    _bufferIsUpdated(false),
    _critSect(critSect),
    _streamId(0),
    _zOrder(0),
    _startWidth(0),
    _startHeight(0),
    _stopWidth(0),
    _stopHeight(0)
{

}

D3D9Channel::~D3D9Channel()
{
    //release the texture
    if (_pTexture != NULL)
    {
        _pTexture->Release();
        _pTexture = NULL;
    }
}

void D3D9Channel::SetStreamSettings(uint16_t streamId,
                                        uint32_t zOrder,
                                        float startWidth,
                                        float startHeight,
                                        float stopWidth,
                                        float stopHeight)
{
    _streamId = streamId;
    _zOrder = zOrder;
    _startWidth = startWidth;
    _startHeight = startHeight;
    _stopWidth = stopWidth;
    _stopHeight = stopHeight;
}

int D3D9Channel::GetStreamSettings(uint16_t streamId,
                                       uint32_t& zOrder,
                                       float& startWidth,
                                       float& startHeight,
                                       float& stopWidth,
                                       float& stopHeight)
{
    streamId = _streamId;
    zOrder = _zOrder;
    startWidth = _startWidth;
    startHeight = _startHeight;
    stopWidth = _stopWidth;
    stopHeight = _stopHeight;
    return 0;
}

int D3D9Channel::GetTextureWidth()
{
    return _width;
}

int D3D9Channel::GetTextureHeight()
{
    return _height;
}

// Called from video engine when a the frame size changed
int D3D9Channel::FrameSizeChange(int width, int height, int numberOfStreams)
{
    WEBRTC_TRACE(kTraceInfo, kTraceVideo, -1,
                 "FrameSizeChange, wifth: %d, height: %d, streams: %d", width,
                 height, numberOfStreams);

    CriticalSectionScoped cs(_critSect);
    _width = width;
    _height = height;

    //clean the previous texture
    if (_pTexture != NULL)
    {
        _pTexture->Release();
        _pTexture = NULL;
    }

    HRESULT ret = E_POINTER;

    if (_pd3dDevice)
      ret = _pd3dDevice->CreateTexture(_width, _height, 1, 0, D3DFMT_A8R8G8B8,
                                       D3DPOOL_MANAGED, &_pTexture, NULL);

    if (FAILED(ret))
    {
        _pTexture = NULL;
        return -1;
    }

    return 0;
}

int32_t D3D9Channel::RenderFrame(const uint32_t streamId,
                                 const VideoFrame& videoFrame) {
    CriticalSectionScoped cs(_critSect);
    if (_width != videoFrame.width() || _height != videoFrame.height())
    {
        if (FrameSizeChange(videoFrame.width(), videoFrame.height(), 1) == -1)
        {
            return -1;
        }
    }
    return DeliverFrame(videoFrame);
}

// Called from video engine when a new frame should be rendered.
int D3D9Channel::DeliverFrame(const VideoFrame& videoFrame) {
  WEBRTC_TRACE(kTraceStream, kTraceVideo, -1,
               "DeliverFrame to D3D9Channel");

  CriticalSectionScoped cs(_critSect);

  // FIXME if _bufferIsUpdated is still true (not be renderred), do we want to
  // update the texture? probably not
  if (_bufferIsUpdated) {
    WEBRTC_TRACE(kTraceStream, kTraceVideo, -1,
                 "Last frame hasn't been rendered yet. Drop this frame.");
    return -1;
  }

  if (!_pd3dDevice) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, -1,
                 "D3D for rendering not initialized.");
    return -1;
  }

  if (!_pTexture) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, -1,
                 "Texture for rendering not initialized.");
    return -1;
  }

  D3DLOCKED_RECT lr;

  if (FAILED(_pTexture->LockRect(0, &lr, NULL, 0))) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, -1,
                 "Failed to lock a texture in D3D9 Channel.");
    return -1;
  }
  UCHAR* pRect = (UCHAR*) lr.pBits;

  ConvertFromI420(videoFrame, kARGB, 0, pRect);

  if (FAILED(_pTexture->UnlockRect(0))) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, -1,
                 "Failed to unlock a texture in D3D9 Channel.");
    return -1;
  }

  _bufferIsUpdated = true;
  return 0;
}

// Called by d3d channel owner to indicate the frame/texture has been rendered off
int D3D9Channel::RenderOffFrame()
{
    WEBRTC_TRACE(kTraceStream, kTraceVideo, -1,
                 "Frame has been rendered to the screen.");
    CriticalSectionScoped cs(_critSect);
    _bufferIsUpdated = false;
    return 0;
}

// Called by d3d channel owner to check if the texture is updated
int D3D9Channel::IsUpdated(bool& isUpdated)
{
    CriticalSectionScoped cs(_critSect);
    isUpdated = _bufferIsUpdated;
    return 0;
}

// Called by d3d channel owner to get the texture
LPDIRECT3DTEXTURE9 D3D9Channel::GetTexture()
{
    CriticalSectionScoped cs(_critSect);
    return _pTexture;
}

int D3D9Channel::ReleaseTexture()
{
    CriticalSectionScoped cs(_critSect);

    //release the texture
    if (_pTexture != NULL)
    {
        _pTexture->Release();
        _pTexture = NULL;
    }
    _pd3dDevice = NULL;
    return 0;
}

int D3D9Channel::RecreateTexture(LPDIRECT3DDEVICE9 pd3DDevice)
{
    CriticalSectionScoped cs(_critSect);

    _pd3dDevice = pd3DDevice;

    if (_pTexture != NULL)
    {
        _pTexture->Release();
        _pTexture = NULL;
    }

    HRESULT ret;

    ret = _pd3dDevice->CreateTexture(_width, _height, 1, 0, D3DFMT_A8R8G8B8,
                                     D3DPOOL_MANAGED, &_pTexture, NULL);

    if (FAILED(ret))
    {
        _pTexture = NULL;
        return -1;
    }

    return 0;
}

/*
 *
 *    VideoRenderDirect3D9
 *
 */
VideoRenderDirect3D9::VideoRenderDirect3D9(Trace* trace,
                                                   HWND hWnd,
                                                   bool fullScreen) :
    _refD3DCritsect(*CriticalSectionWrapper::CreateCriticalSection()),
    _trace(trace),
    _hWnd(hWnd),
    _fullScreen(fullScreen),
    _pTextureLogo(NULL),
    _pVB(NULL),
    _pd3dDevice(NULL),
    _pD3D(NULL),
    _d3dChannels(),
    _d3dZorder(),
    _screenUpdateEvent(NULL),
    _logoLeft(0),
    _logoTop(0),
    _logoRight(0),
    _logoBottom(0),
    _pd3dSurface(NULL),
    _totalMemory(0),
    _availableMemory(0)
{
    _screenUpdateThread.reset(new rtc::PlatformThread(
        ScreenUpdateThreadProc, this, "ScreenUpdateThread"));
    _screenUpdateEvent = EventTimerWrapper::Create();
    SetRect(&_originalHwndRect, 0, 0, 0, 0);
}

VideoRenderDirect3D9::~VideoRenderDirect3D9()
{
    //NOTE: we should not enter CriticalSection in here!

    // Signal event to exit thread, then delete it
    rtc::PlatformThread* tmpPtr = _screenUpdateThread.release();
    if (tmpPtr)
    {
        _screenUpdateEvent->Set();
        _screenUpdateEvent->StopTimer();

        tmpPtr->Stop();
        delete tmpPtr;
    }
    delete _screenUpdateEvent;

    //close d3d device
    CloseDevice();

    // Delete all channels
    std::map<int, D3D9Channel*>::iterator it = _d3dChannels.begin();
    while (it != _d3dChannels.end())
    {
        delete it->second;
        it = _d3dChannels.erase(it);
    }
    // Clean the zOrder map
    _d3dZorder.clear();

    if (_fullScreen)
    {
        // restore hwnd to original size and position
        ::SetWindowPos(_hWnd, HWND_NOTOPMOST, _originalHwndRect.left,
                       _originalHwndRect.top, _originalHwndRect.right
                               - _originalHwndRect.left,
                       _originalHwndRect.bottom - _originalHwndRect.top,
                       SWP_FRAMECHANGED);
        ::RedrawWindow(_hWnd, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW
                | RDW_ERASE);
        ::RedrawWindow(NULL, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW
                | RDW_ERASE);
    }

    delete &_refD3DCritsect;
}

DWORD VideoRenderDirect3D9::GetVertexProcessingCaps()
{
    D3DCAPS9 caps;
    DWORD dwVertexProcessing = D3DCREATE_SOFTWARE_VERTEXPROCESSING;
    if (SUCCEEDED(_pD3D->GetDeviceCaps(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL,
                                       &caps)))
    {
        if ((caps.DevCaps & D3DDEVCAPS_HWTRANSFORMANDLIGHT)
                == D3DDEVCAPS_HWTRANSFORMANDLIGHT)
        {
            dwVertexProcessing = D3DCREATE_HARDWARE_VERTEXPROCESSING;
        }
    }
    return dwVertexProcessing;
}

int VideoRenderDirect3D9::InitializeD3D(HWND hWnd,
                                            D3DPRESENT_PARAMETERS* pd3dpp)
{
    // initialize Direct3D
    if (NULL == (_pD3D = Direct3DCreate9(D3D_SDK_VERSION)))
    {
        return -1;
    }

    // determine what type of vertex processing to use based on the device capabilities
    DWORD dwVertexProcessing = GetVertexProcessingCaps();

    // get the display mode
    D3DDISPLAYMODE d3ddm;
    _pD3D->GetAdapterDisplayMode(D3DADAPTER_DEFAULT, &d3ddm);
    pd3dpp->BackBufferFormat = d3ddm.Format;

    // create the D3D device
    if (FAILED(_pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWnd,
                                   dwVertexProcessing | D3DCREATE_MULTITHREADED
                                           | D3DCREATE_FPU_PRESERVE, pd3dpp,
                                   &_pd3dDevice)))
    {
        //try the ref device
        if (FAILED(_pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_REF,
                                       hWnd, dwVertexProcessing
                                               | D3DCREATE_MULTITHREADED
                                               | D3DCREATE_FPU_PRESERVE,
                                       pd3dpp, &_pd3dDevice)))
        {
            return -1;
        }
    }

    return 0;
}

int VideoRenderDirect3D9::ResetDevice()
{
    WEBRTC_TRACE(kTraceInfo, kTraceVideo, -1,
                 "VideoRenderDirect3D9::ResetDevice");

    CriticalSectionScoped cs(&_refD3DCritsect);

    //release the channel texture
    std::map<int, D3D9Channel*>::iterator it;
    it = _d3dChannels.begin();
    while (it != _d3dChannels.end())
    {
        if (it->second)
        {
            it->second->ReleaseTexture();
        }
        it++;
    }

    //close d3d device
    if (CloseDevice() != 0)
    {
        WEBRTC_TRACE(kTraceError, kTraceVideo, -1,
                     "VideoRenderDirect3D9::ResetDevice failed to CloseDevice");
        return -1;
    }

    //reinit d3d device
    if (InitDevice() != 0)
    {
        WEBRTC_TRACE(kTraceError, kTraceVideo, -1,
                     "VideoRenderDirect3D9::ResetDevice failed to InitDevice");
        return -1;
    }

    //recreate channel texture
    it = _d3dChannels.begin();
    while (it != _d3dChannels.end())
    {
        if (it->second)
        {
            it->second->RecreateTexture(_pd3dDevice);
        }
        it++;
    }

    return 0;
}

int VideoRenderDirect3D9::InitDevice()
{
    // Set up the structure used to create the D3DDevice
    ZeroMemory(&_d3dpp, sizeof(_d3dpp));
    _d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    _d3dpp.BackBufferFormat = D3DFMT_A8R8G8B8;
    if (GetWindowRect(_hWnd, &_originalHwndRect) == 0)
    {
        WEBRTC_TRACE(kTraceError, kTraceVideo, -1,
                     "VideoRenderDirect3D9::InitDevice Could not get window size");
        return -1;
    }
    if (!_fullScreen)
    {
        _winWidth = _originalHwndRect.right - _originalHwndRect.left;
        _winHeight = _originalHwndRect.bottom - _originalHwndRect.top;
        _d3dpp.Windowed = TRUE;
        _d3dpp.BackBufferHeight = 0;
        _d3dpp.BackBufferWidth = 0;
    }
    else
    {
        _winWidth = (LONG) ::GetSystemMetrics(SM_CXSCREEN);
        _winHeight = (LONG) ::GetSystemMetrics(SM_CYSCREEN);
        _d3dpp.Windowed = FALSE;
        _d3dpp.BackBufferWidth = _winWidth;
        _d3dpp.BackBufferHeight = _winHeight;
        _d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
    }

    if (InitializeD3D(_hWnd, &_d3dpp) == -1)
    {
        WEBRTC_TRACE(kTraceError, kTraceVideo, -1,
                     "VideoRenderDirect3D9::InitDevice failed in InitializeD3D");
        return -1;
    }

    // Turn off culling, so we see the front and back of the triangle
    _pd3dDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);

    // Turn off D3D lighting, since we are providing our own vertex colors
    _pd3dDevice->SetRenderState(D3DRS_LIGHTING, FALSE);

    // Settings for alpha blending
    _pd3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    _pd3dDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    _pd3dDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);

    _pd3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
    _pd3dDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
    _pd3dDevice->SetSamplerState( 0, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR );

    // Initialize Vertices
    CUSTOMVERTEX Vertices[] = {
            //front
            { -1.0f, -1.0f, 0.0f, 0xffffffff, 0, 1 }, { -1.0f, 1.0f, 0.0f,
                    0xffffffff, 0, 0 },
            { 1.0f, -1.0f, 0.0f, 0xffffffff, 1, 1 }, { 1.0f, 1.0f, 0.0f,
                    0xffffffff, 1, 0 } };

    // Create the vertex buffer.
    if (FAILED(_pd3dDevice->CreateVertexBuffer(sizeof(Vertices), 0,
                                               D3DFVF_CUSTOMVERTEX,
                                               D3DPOOL_DEFAULT, &_pVB, NULL )))
    {
        WEBRTC_TRACE(kTraceError, kTraceVideo, -1,
                     "Failed to create the vertex buffer.");
        return -1;
    }

    // Now we fill the vertex buffer.
    VOID* pVertices;
    if (FAILED(_pVB->Lock(0, sizeof(Vertices), (void**) &pVertices, 0)))
    {
        WEBRTC_TRACE(kTraceError, kTraceVideo, -1,
                     "Failed to lock the vertex buffer.");
        return -1;
    }
    memcpy(pVertices, Vertices, sizeof(Vertices));
    _pVB->Unlock();

    return 0;
}

int32_t VideoRenderDirect3D9::Init()
{
    WEBRTC_TRACE(kTraceInfo, kTraceVideo, -1,
                 "VideoRenderDirect3D9::Init");

    CriticalSectionScoped cs(&_refD3DCritsect);

    // Start rendering thread...
    if (!_screenUpdateThread)
    {
        WEBRTC_TRACE(kTraceError, kTraceVideo, -1, "Thread not created");
        return -1;
    }
    _screenUpdateThread->Start();
    _screenUpdateThread->SetPriority(rtc::kRealtimePriority);

    // Start the event triggering the render process
    unsigned int monitorFreq = 60;
    DEVMODE dm;
    // initialize the DEVMODE structure
    ZeroMemory(&dm, sizeof(dm));
    dm.dmSize = sizeof(dm);
    if (0 != EnumDisplaySettings(NULL, ENUM_CURRENT_SETTINGS, &dm))
    {
        monitorFreq = dm.dmDisplayFrequency;
    }
    _screenUpdateEvent->StartTimer(true, 1000 / monitorFreq);

    return InitDevice();
}

int32_t VideoRenderDirect3D9::ChangeWindow(void* window)
{
    WEBRTC_TRACE(kTraceError, kTraceVideo, -1, "Not supported.");
    return -1;
}

int VideoRenderDirect3D9::UpdateRenderSurface()
{
    CriticalSectionScoped cs(&_refD3DCritsect);

    // Check if there are any updated buffers
    bool updated = false;
    std::map<int, D3D9Channel*>::iterator it;
    it = _d3dChannels.begin();
    while (it != _d3dChannels.end())
    {

        D3D9Channel* channel = it->second;
        channel->IsUpdated(updated);
        if (updated)
        {
            break;
        }
        it++;
    }
    //nothing is updated, continue
    if (!updated)
        return -1;

    // Clear the backbuffer to a black color
    _pd3dDevice->Clear(0, NULL, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 0, 0), 1.0f,
                       0);

    // Begin the scene
    if (SUCCEEDED(_pd3dDevice->BeginScene()))
    {
        _pd3dDevice->SetStreamSource(0, _pVB, 0, sizeof(CUSTOMVERTEX));
        _pd3dDevice->SetFVF(D3DFVF_CUSTOMVERTEX);

        //draw all the channels
        //get texture from the channels
        LPDIRECT3DTEXTURE9 textureFromChannel = NULL;
        DWORD textureWidth, textureHeight;

        std::multimap<int, unsigned int>::reverse_iterator it;
        it = _d3dZorder.rbegin();
        while (it != _d3dZorder.rend())
        {
            // loop through all channels and streams in Z order
            int channel = it->second & 0x0000ffff;

            std::map<int, D3D9Channel*>::iterator ddIt;
            ddIt = _d3dChannels.find(channel);
            if (ddIt != _d3dChannels.end())
            {
                // found the channel
                D3D9Channel* channelObj = ddIt->second;
                if (channelObj)
                {
                    textureFromChannel = channelObj->GetTexture();
                    textureWidth = channelObj->GetTextureWidth();
                    textureHeight = channelObj->GetTextureHeight();

                    uint32_t zOrder;
                    float startWidth, startHeight, stopWidth, stopHeight;
                    channelObj->GetStreamSettings(0, zOrder, startWidth,
                                                  startHeight, stopWidth,
                                                  stopHeight);

                    //draw the video stream
                    UpdateVerticeBuffer(_pVB, 0, startWidth, startHeight,
                                        stopWidth, stopHeight);
                    _pd3dDevice->SetTexture(0, textureFromChannel);
                    _pd3dDevice->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);

                    //Notice channel that this frame as been rendered
                    channelObj->RenderOffFrame();
                }
            }
            it++;
        }

        //draw the logo
        if (_pTextureLogo)
        {
            UpdateVerticeBuffer(_pVB, 0, _logoLeft, _logoTop, _logoRight,
                                _logoBottom);
            _pd3dDevice->SetTexture(0, _pTextureLogo);
            _pd3dDevice->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);
        }

        // End the scene
        _pd3dDevice->EndScene();
    }

    // Present the backbuffer contents to the display
    _pd3dDevice->Present(NULL, NULL, NULL, NULL );

    return 0;
}

//set the  alpha value of the pixal with a particular colorkey as 0
int VideoRenderDirect3D9::SetTransparentColor(LPDIRECT3DTEXTURE9 pTexture,
                                                  DDCOLORKEY* transparentColorKey,
                                                  DWORD width,
                                                  DWORD height)
{
    D3DLOCKED_RECT lr;
    if (!pTexture)
        return -1;

    CriticalSectionScoped cs(&_refD3DCritsect);
    if (SUCCEEDED(pTexture->LockRect(0, &lr, NULL, D3DLOCK_DISCARD)))
    {
        for (DWORD y = 0; y < height; y++)
        {
            DWORD dwOffset = y * width;

            for (DWORD x = 0; x < width; x)
            {
                DWORD temp = ((DWORD*) lr.pBits)[dwOffset + x];
                if ((temp & 0x00FFFFFF)
                        == transparentColorKey->dwColorSpaceLowValue)
                {
                    temp &= 0x00FFFFFF;
                }
                else
                {
                    temp |= 0xFF000000;
                }
                ((DWORD*) lr.pBits)[dwOffset + x] = temp;
                x++;
            }
        }
        pTexture->UnlockRect(0);
        return 0;
    }
    return -1;
}

/*
 *
 *    Rendering process
 *
 */
bool VideoRenderDirect3D9::ScreenUpdateThreadProc(void* obj)
{
    return static_cast<VideoRenderDirect3D9*> (obj)->ScreenUpdateProcess();
}

bool VideoRenderDirect3D9::ScreenUpdateProcess()
{
    _screenUpdateEvent->Wait(100);

    if (!_screenUpdateThread)
    {
        //stop the thread
        return false;
    }
    if (!_pd3dDevice)
    {
        WEBRTC_TRACE(kTraceError, kTraceVideo, -1,
                     "d3dDevice not created.");
        return true;
    }

    HRESULT hr = _pd3dDevice->TestCooperativeLevel();

    if (SUCCEEDED(hr))
    {
        UpdateRenderSurface();
    }

    if (hr == D3DERR_DEVICELOST)
    {
        //Device is lost and cannot be reset yet

    }
    else if (hr == D3DERR_DEVICENOTRESET)
    {
        //Lost but we can reset it now
        //Note: the standard way is to call Reset, however for some reason doesn't work here.
        //so we will release the device and create it again.
        ResetDevice();
    }

    return true;
}

int VideoRenderDirect3D9::CloseDevice()
{
    CriticalSectionScoped cs(&_refD3DCritsect);
    WEBRTC_TRACE(kTraceInfo, kTraceVideo, -1,
                 "VideoRenderDirect3D9::CloseDevice");

    if (_pTextureLogo != NULL)
    {
        _pTextureLogo->Release();
        _pTextureLogo = NULL;
    }

    if (_pVB != NULL)
    {
        _pVB->Release();
        _pVB = NULL;
    }

    if (_pd3dDevice != NULL)
    {
        _pd3dDevice->Release();
        _pd3dDevice = NULL;
    }

    if (_pD3D != NULL)
    {
        _pD3D->Release();
        _pD3D = NULL;
    }

    if (_pd3dSurface != NULL)
        _pd3dSurface->Release();
    return 0;
}

D3D9Channel* VideoRenderDirect3D9::GetD3DChannel(int channel)
{
    std::map<int, D3D9Channel*>::iterator ddIt;
    ddIt = _d3dChannels.find(channel & 0x0000ffff);
    D3D9Channel* ddobj = NULL;
    if (ddIt != _d3dChannels.end())
    {
        ddobj = ddIt->second;
    }
    if (ddobj == NULL)
    {
        WEBRTC_TRACE(kTraceError, kTraceVideo, -1,
                     "Direct3D render failed to find channel");
        return NULL;
    }
    return ddobj;
}

int32_t VideoRenderDirect3D9::DeleteChannel(const uint32_t streamId)
{
    CriticalSectionScoped cs(&_refD3DCritsect);


    std::multimap<int, unsigned int>::iterator it;
    it = _d3dZorder.begin();
    while (it != _d3dZorder.end())
    {
        if ((streamId & 0x0000ffff) == (it->second & 0x0000ffff))
        {
            it = _d3dZorder.erase(it);
            break;
        }
        it++;
    }

    std::map<int, D3D9Channel*>::iterator ddIt;
    ddIt = _d3dChannels.find(streamId & 0x0000ffff);
    if (ddIt != _d3dChannels.end())
    {
        delete ddIt->second;
        _d3dChannels.erase(ddIt);
        return 0;
    }
    return -1;
}

VideoRenderCallback* VideoRenderDirect3D9::CreateChannel(const uint32_t channel,
                                                                 const uint32_t zOrder,
                                                                 const float left,
                                                                 const float top,
                                                                 const float right,
                                                                 const float bottom)
{
    CriticalSectionScoped cs(&_refD3DCritsect);

    //FIXME this should be done in VideoAPIWindows? stop the frame deliver first
    //remove the old channel
    DeleteChannel(channel);

    D3D9Channel* d3dChannel = new D3D9Channel(_pd3dDevice,
                                                      &_refD3DCritsect, _trace);
    d3dChannel->SetStreamSettings(0, zOrder, left, top, right, bottom);

    // store channel
    _d3dChannels[channel & 0x0000ffff] = d3dChannel;

    // store Z order
    // default streamID is 0
    _d3dZorder.insert(
                      std::pair<int, unsigned int>(zOrder, channel & 0x0000ffff));

    return d3dChannel;
}

int32_t VideoRenderDirect3D9::GetStreamSettings(const uint32_t channel,
                                                const uint16_t streamId,
                                                uint32_t& zOrder,
                                                float& left, float& top,
                                                float& right, float& bottom)
{
    std::map<int, D3D9Channel*>::iterator ddIt;
    ddIt = _d3dChannels.find(channel & 0x0000ffff);
    D3D9Channel* ddobj = NULL;
    if (ddIt != _d3dChannels.end())
    {
        ddobj = ddIt->second;
    }
    if (ddobj == NULL)
    {
        WEBRTC_TRACE(kTraceError, kTraceVideo, -1,
                     "Direct3D render failed to find channel");
        return -1;
    }
    // Only allow one stream per channel, demuxing is
    return ddobj->GetStreamSettings(0, zOrder, left, top, right, bottom);
}

int VideoRenderDirect3D9::UpdateVerticeBuffer(LPDIRECT3DVERTEXBUFFER9 pVB,
                                                  int offset,
                                                  float startWidth,
                                                  float startHeight,
                                                  float stopWidth,
                                                  float stopHeight)
{
    if (pVB == NULL)
        return -1;

    float left, right, top, bottom;

    //update the vertice buffer
    //0,1 => -1,1
    left = startWidth * 2 - 1;
    right = stopWidth * 2 - 1;

    //0,1 => 1,-1
    top = 1 - startHeight * 2;
    bottom = 1 - stopHeight * 2;

    CUSTOMVERTEX newVertices[] = {
            //logo
            { left, bottom, 0.0f, 0xffffffff, 0, 1 }, { left, top, 0.0f,
                    0xffffffff, 0, 0 },
            { right, bottom, 0.0f, 0xffffffff, 1, 1 }, { right, top, 0.0f,
                    0xffffffff, 1, 0 }, };
    // Now we fill the vertex buffer.
    VOID* pVertices;
    if (FAILED(pVB->Lock(sizeof(CUSTOMVERTEX) * offset, sizeof(newVertices),
                         (void**) &pVertices, 0)))
    {
        WEBRTC_TRACE(kTraceError, kTraceVideo, -1,
                     "Failed to lock the vertex buffer.");
        return -1;
    }
    memcpy(pVertices, newVertices, sizeof(newVertices));
    pVB->Unlock();

    return 0;
}

int32_t VideoRenderDirect3D9::StartRender()
{
    WEBRTC_TRACE(kTraceError, kTraceVideo, -1, "Not supported.");
    return 0;
}

int32_t VideoRenderDirect3D9::StopRender()
{
    WEBRTC_TRACE(kTraceError, kTraceVideo, -1, "Not supported.");
    return 0;
}

bool VideoRenderDirect3D9::IsFullScreen()
{
    return _fullScreen;
}

int32_t VideoRenderDirect3D9::SetCropping(const uint32_t channel,
                                          const uint16_t streamId,
                                          const float left, const float top,
                                          const float right, const float bottom)
{
    WEBRTC_TRACE(kTraceError, kTraceVideo, -1, "Not supported.");
    return 0;
}

int32_t VideoRenderDirect3D9::SetTransparentBackground(
                                                                 const bool enable)
{
    WEBRTC_TRACE(kTraceError, kTraceVideo, -1, "Not supported.");
    return 0;
}

int32_t VideoRenderDirect3D9::SetText(const uint8_t textId,
                                      const uint8_t* text,
                                      const int32_t textLength,
                                      const uint32_t colorText,
                                      const uint32_t colorBg,
                                      const float left, const float top,
                                      const float rigth, const float bottom)
{
    WEBRTC_TRACE(kTraceError, kTraceVideo, -1, "Not supported.");
    return 0;
}

int32_t VideoRenderDirect3D9::SetBitmap(const void* bitMap,
                                        const uint8_t pictureId,
                                        const void* colorKey,
                                        const float left, const float top,
                                        const float right, const float bottom)
{
    if (!bitMap)
    {
        if (_pTextureLogo != NULL)
        {
            _pTextureLogo->Release();
            _pTextureLogo = NULL;
        }
        WEBRTC_TRACE(kTraceInfo, kTraceVideo, -1, "Remove bitmap.");
        return 0;
    }

    // sanity
    if (left > 1.0f || left < 0.0f ||
        top > 1.0f || top < 0.0f ||
        right > 1.0f || right < 0.0f ||
        bottom > 1.0f || bottom < 0.0f)
    {
        WEBRTC_TRACE(kTraceError, kTraceVideo, -1,
                     "Direct3D SetBitmap invalid parameter");
        return -1;
    }

    if ((bottom <= top) || (right <= left))
    {
        WEBRTC_TRACE(kTraceError, kTraceVideo, -1,
                     "Direct3D SetBitmap invalid parameter");
        return -1;
    }

    CriticalSectionScoped cs(&_refD3DCritsect);

    unsigned char* srcPtr;
    HGDIOBJ oldhand;
    BITMAPINFO pbi;
    BITMAP bmap;
    HDC hdcNew;
    hdcNew = CreateCompatibleDC(0);
    // Fill out the BITMAP structure.
    GetObject((HBITMAP)bitMap, sizeof(bmap), &bmap);
    //Select the bitmap handle into the new device context.
    oldhand = SelectObject(hdcNew, (HGDIOBJ) bitMap);
    // we are done with this object
    DeleteObject(oldhand);
    pbi.bmiHeader.biSize = 40;
    pbi.bmiHeader.biWidth = bmap.bmWidth;
    pbi.bmiHeader.biHeight = bmap.bmHeight;
    pbi.bmiHeader.biPlanes = 1;
    pbi.bmiHeader.biBitCount = bmap.bmBitsPixel;
    pbi.bmiHeader.biCompression = BI_RGB;
    pbi.bmiHeader.biSizeImage = bmap.bmWidth * bmap.bmHeight * 3;
    srcPtr = new unsigned char[bmap.bmWidth * bmap.bmHeight * 4];
    // the original un-stretched image in RGB24
    int pixelHeight = GetDIBits(hdcNew, (HBITMAP)bitMap, 0, bmap.bmHeight, srcPtr, &pbi,
                                DIB_RGB_COLORS);
    if (pixelHeight == 0)
    {
        WEBRTC_TRACE(kTraceError, kTraceVideo, -1,
                     "Direct3D failed to GetDIBits in SetBitmap");
        delete[] srcPtr;
        return -1;
    }
    DeleteDC(hdcNew);
    if (pbi.bmiHeader.biBitCount != 24 && pbi.bmiHeader.biBitCount != 32)
    {
        WEBRTC_TRACE(kTraceError, kTraceVideo, -1,
                     "Direct3D failed to SetBitmap invalid bit depth");
        delete[] srcPtr;
        return -1;
    }

    HRESULT ret;
    //release the previous logo texture
    if (_pTextureLogo != NULL)
    {
        _pTextureLogo->Release();
        _pTextureLogo = NULL;
    }
    ret = _pd3dDevice->CreateTexture(bmap.bmWidth, bmap.bmHeight, 1, 0,
                                     D3DFMT_A8R8G8B8, D3DPOOL_MANAGED,
                                     &_pTextureLogo, NULL);
    if (FAILED(ret))
    {
        _pTextureLogo = NULL;
        delete[] srcPtr;
        return -1;
    }
    if (!_pTextureLogo)
    {
        WEBRTC_TRACE(kTraceError, kTraceVideo, -1,
                     "Texture for rendering not initialized.");
        delete[] srcPtr;
        return -1;
    }

    D3DLOCKED_RECT lr;
    if (FAILED(_pTextureLogo->LockRect(0, &lr, NULL, 0)))
    {
        delete[] srcPtr;
        return -1;
    }
    unsigned char* dstPtr = (UCHAR*) lr.pBits;
    int pitch = bmap.bmWidth * 4;

    if (pbi.bmiHeader.biBitCount == 24)
    {
        ConvertRGB24ToARGB(srcPtr, dstPtr, bmap.bmWidth, bmap.bmHeight, 0);
    }
    else
    {
        unsigned char* srcTmp = srcPtr + (bmap.bmWidth * 4) * (bmap.bmHeight - 1);
        for (int i = 0; i < bmap.bmHeight; ++i)
        {
            memcpy(dstPtr, srcTmp, bmap.bmWidth * 4);
            srcTmp -= bmap.bmWidth * 4;
            dstPtr += pitch;
        }
    }

    delete[] srcPtr;
    if (FAILED(_pTextureLogo->UnlockRect(0)))
    {
        return -1;
    }

    if (colorKey)
    {
        DDCOLORKEY* ddColorKey =
                static_cast<DDCOLORKEY*> (const_cast<void*> (colorKey));
        SetTransparentColor(_pTextureLogo, ddColorKey, bmap.bmWidth,
                            bmap.bmHeight);
    }

    //update the vertice buffer
    //0,1 => -1,1
    _logoLeft = left;
    _logoRight = right;

    //0,1 => 1,-1
    _logoTop = top;
    _logoBottom = bottom;

    return 0;

}

int32_t VideoRenderDirect3D9::GetGraphicsMemory(uint64_t& totalMemory,
                                                uint64_t& availableMemory)
{
    if (_totalMemory == -1 || _availableMemory == -1)
    {
        totalMemory = 0;
        availableMemory = 0;
        return -1;
    }
    totalMemory = _totalMemory;
    availableMemory = _availableMemory;
    return 0;
}

int32_t VideoRenderDirect3D9::ConfigureRenderer(const uint32_t channel,
                                                const uint16_t streamId,
                                                const unsigned int zOrder,
                                                const float left,
                                                const float top,
                                                const float right,
                                                const float bottom)
{
    std::map<int, D3D9Channel*>::iterator ddIt;
    ddIt = _d3dChannels.find(channel & 0x0000ffff);
    D3D9Channel* ddobj = NULL;
    if (ddIt != _d3dChannels.end())
    {
        ddobj = ddIt->second;
    }
    if (ddobj == NULL)
    {
        WEBRTC_TRACE(kTraceError, kTraceVideo, -1,
                     "Direct3D render failed to find channel");
        return -1;
    }
    // Only allow one stream per channel, demuxing is
    ddobj->SetStreamSettings(0, zOrder, left, top, right, bottom);

    return 0;
}

}  // namespace webrtc
