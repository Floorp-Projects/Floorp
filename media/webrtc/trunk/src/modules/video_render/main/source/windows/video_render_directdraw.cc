/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video_render_directdraw.h"
#include "video_render_windows_impl.h"
#include "Windows.h"
#include <ddraw.h>
#include <assert.h>
#include <initguid.h>
#include <MMSystem.h> // timeGetTime
DEFINE_GUID( IID_IDirectDraw7,0x15e65ec0,0x3b9c,0x11d2,0xb9,0x2f,0x00,0x60,0x97,0x97,0xea,0x5b );

#include "thread_wrapper.h"
#include "event_wrapper.h"
#include "trace.h"
#include "critical_section_wrapper.h"
//#include "VideoErrors.h"

// Added
#include "module_common_types.h"

#pragma warning(disable: 4355) // 'this' : used in base member initializer list
// picture in picture do we need overlay? answer no we can blit directly
// conference is easy since we can blt the quadrants seperatly

// To determine if the driver supports DMA, retrieve the driver capabilities by calling the IDirectDraw::GetCaps method, 
// then look for DDBLTCAPS_READSYSMEM and/or DDBLTCAPS_WRITESYSMEM. If either of these flags is set, the device supports DMA.
// Blt with SRCCOPY should do this can we use it?
// investigate DDLOCK_NOSYSLOCK 

namespace webrtc {

#define EXTRACT_BITS_RL(the_val, bits_start, bits_len) ((the_val >> (bits_start - 1)) & ((1 << bits_len) - 1)) 

WindowsThreadCpuUsage::WindowsThreadCpuUsage() :
    _lastGetCpuUsageTime(0),
    _lastCpuUsageTime(0),
    _hThread(::GetCurrentThread()),
    _cores(0),
    _lastCpuUsage(0)
{

    DWORD_PTR pmask, smask;
    DWORD access = PROCESS_QUERY_INFORMATION;
    if (GetProcessAffinityMask(
                               OpenProcess(access, false, GetCurrentProcessId()),
                               &pmask, &smask) != 0)
    {

        for (int i = 1; i < 33; i++)
        {
            if (EXTRACT_BITS_RL(pmask,i,1) == 0)
            {
                break;
            }
            _cores++;
        }
        //sanity
        if (_cores > 32)
        {
            _cores = 32;
        }
        if (_cores < 1)
        {
            _cores = 1;
        }
    }
    else
    {
        _cores = 1;
    }
    GetCpuUsage();
}

//in % since last call
int WindowsThreadCpuUsage::GetCpuUsage()
{
    DWORD now = timeGetTime();

    _int64 newTime = 0;
    FILETIME creationTime;
    FILETIME exitTime;
    _int64 kernelTime = 0;
    _int64 userTime = 0;
    if (GetThreadTimes(_hThread, (FILETIME*) &creationTime, &exitTime,
                       (FILETIME*) &kernelTime, (FILETIME*) &userTime) != 0)
    {
        newTime = (kernelTime + userTime);
    }
    if (newTime == 0)
    {
        _lastGetCpuUsageTime = now;
        return _lastCpuUsage;
    }

    // calculate the time difference since last call
    const DWORD diffTime = (now - _lastGetCpuUsageTime);
    _lastGetCpuUsageTime = now;

    if (newTime < _lastCpuUsageTime)
    {
        _lastCpuUsageTime = newTime;
        return _lastCpuUsage;
    }
    const int cpuDiff = (int) (newTime - _lastCpuUsageTime) / 10000;
    _lastCpuUsageTime = newTime;

    // calculate the CPU usage

    _lastCpuUsage = (int) (float((cpuDiff * 100)) / (diffTime * _cores) + 0.5f);

    if (_lastCpuUsage > 100)
    {
        _lastCpuUsage = 100;
    }
    return _lastCpuUsage;

}

DirectDrawStreamSettings::DirectDrawStreamSettings() :
    _startWidth(0.0F),
    _stopWidth(1.0F),
    _startHeight(0.0F),
    _stopHeight(1.0F),
    _cropStartWidth(0.0F),
    _cropStopWidth(1.0F),
    _cropStartHeight(0.0F),
    _cropStopHeight(1.0F)
{
}
;

DirectDrawBitmapSettings::DirectDrawBitmapSettings() :
    _transparentBitMap(NULL),
    _transparentBitmapLeft(0.0f),
    _transparentBitmapRight(1.0f),
    _transparentBitmapTop(0.0f),
    _transparentBitmapBottom(1.0f),
    _transparentBitmapWidth(0),
    _transparentBitmapHeight(0),
    _transparentBitmapColorKey(NULL),
    _transparentBitmapSurface(NULL)
{
}
;

DirectDrawBitmapSettings::~DirectDrawBitmapSettings()
{
    if (_transparentBitmapColorKey)
    {
        delete _transparentBitmapColorKey;
    }
    if (_transparentBitmapSurface)
    {
        _transparentBitmapSurface->Release();
    }
    _transparentBitmapColorKey = NULL;
    _transparentBitmapSurface = NULL;
}
;

int DirectDrawBitmapSettings::SetBitmap(Trace* _trace,
                                            DirectDraw* directDraw)
{
    VideoFrame tempVideoBuffer;
    HGDIOBJ oldhand;
    BITMAPINFO pbi;
    BITMAP bmap;
    HDC hdcNew;

    hdcNew = CreateCompatibleDC(0);

    // Fill out the BITMAP structure.
    GetObject(_transparentBitMap, sizeof(bmap), &bmap);

    //Select the bitmap handle into the new device context.
    oldhand = SelectObject(hdcNew, (HGDIOBJ) _transparentBitMap);

    // we are done with this object
    DeleteObject(oldhand);

    pbi.bmiHeader.biSize = 40;
    pbi.bmiHeader.biWidth = bmap.bmWidth;
    pbi.bmiHeader.biHeight = bmap.bmHeight;
    pbi.bmiHeader.biPlanes = 1;
    pbi.bmiHeader.biBitCount = bmap.bmBitsPixel;
    pbi.bmiHeader.biCompression = BI_RGB;
    pbi.bmiHeader.biSizeImage = bmap.bmWidth * bmap.bmHeight * 3;

    tempVideoBuffer.VerifyAndAllocate(bmap.bmWidth * bmap.bmHeight * 4);

    // the original un-stretched image in RGB24
    // todo is there another struct for pbi purify reports read of 24 bytes larger than size
    int pixelHeight = GetDIBits(hdcNew, _transparentBitMap, 0, bmap.bmHeight,
                                tempVideoBuffer.Buffer(), &pbi, DIB_RGB_COLORS);
    if (pixelHeight == 0)
    {
        WEBRTC_TRACE(kTraceError, kTraceVideo, -1,
                     "DirectDraw failed to GetDIBits in SetBitmap.");
        return -1;
        //return VIDEO_DIRECT_DRAW_FAILURE;
    }

    DeleteDC(hdcNew);

    if (pbi.bmiHeader.biBitCount != 24 && pbi.bmiHeader.biBitCount != 32)
    {
        WEBRTC_TRACE(kTraceError, kTraceVideo, -1,
                     "DirectDraw failed to SetBitmap invalid bit depth");
        return -1;
        //return VIDEO_DIRECT_DRAW_FAILURE;
    }

    DirectDrawSurfaceDesc ddsd;
    memset(&ddsd, 0, sizeof(ddsd));
    ddsd.dwSize = sizeof(ddsd);
    ddsd.dwFlags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH | DDSD_PIXELFORMAT;
    ddsd.ddsCaps.dwCaps = DDSCAPS_VIDEOMEMORY;
    ddsd.dwHeight = bmap.bmHeight;
    ddsd.dwWidth = bmap.bmWidth;

    ddsd.ddpfPixelFormat.dwSize = sizeof(DDPIXELFORMAT);
    ddsd.ddpfPixelFormat.dwFlags = DDPF_RGB;

    _transparentBitmapWidth = bmap.bmWidth;
    _transparentBitmapHeight = bmap.bmHeight;

    ddsd.ddpfPixelFormat.dwRGBBitCount = 32;
    ddsd.ddpfPixelFormat.dwRBitMask = 0xff0000;
    ddsd.ddpfPixelFormat.dwGBitMask = 0xff00;
    ddsd.ddpfPixelFormat.dwBBitMask = 0xff;
    ddsd.ddpfPixelFormat.dwRGBAlphaBitMask = 0;

    if (_transparentBitmapSurface)
    {
        _transparentBitmapSurface->Release();
        _transparentBitmapSurface = NULL;
    }

    HRESULT ddrval =
            directDraw->CreateSurface(&ddsd, &_transparentBitmapSurface, NULL);
    if (FAILED(ddrval))
    {
        WEBRTC_TRACE(
                     kTraceError,
                     kTraceVideo,
                     -1,
                     "DirectDraw failed to CreateSurface _transparentBitmapSurface: 0x%x",
                     ddrval);
        return -1;
        //return VIDEO_DIRECT_DRAW_FAILURE;
    }

    memset(&ddsd, 0, sizeof(DDSURFACEDESC));
    ddsd.dwSize = sizeof(DDSURFACEDESC);
    ddrval = _transparentBitmapSurface->Lock(NULL, &ddsd, DDLOCK_WAIT, NULL);
    if (ddrval == DDERR_SURFACELOST)
    {
        ddrval = _transparentBitmapSurface->Restore();
        if (ddrval != DD_OK)
        {
            WEBRTC_TRACE(kTraceWarning, kTraceVideo, -1,
                         "DirectDraw failed to restore lost _transparentBitmapSurface");
            return -1;
            //return VIDEO_DIRECT_DRAW_FAILURE;
        }
        WEBRTC_TRACE(kTraceInfo, kTraceVideo, -1,
                     "DirectDraw restored lost _transparentBitmapSurface");

        ddrval
                = _transparentBitmapSurface->Lock(NULL, &ddsd, DDLOCK_WAIT,
                                                  NULL);
        if (ddrval != DD_OK)
        {
            WEBRTC_TRACE(
                         kTraceInfo,
                         kTraceVideo,
                         -1,
                         "DirectDraw lock error 0x%x _transparentBitmapSurface",
                         ddrval);
            return -1;
            //return VIDEO_DIRECT_DRAW_FAILURE;
        }
    }
    unsigned char* dstPtr = (unsigned char*) ddsd.lpSurface;
    unsigned char* srcPtr = (unsigned char*) tempVideoBuffer.Buffer();

    int pitch = bmap.bmWidth * 4;
    if (ddsd.dwFlags & DDSD_PITCH)
    {
        pitch = ddsd.lPitch;
    }

    if (pbi.bmiHeader.biBitCount == 24)
    {
        ConvertRGB24ToARGB(srcPtr, dstPtr, bmap.bmWidth, bmap.bmHeight,
                                   0);
    }
    else
    {
        srcPtr += (bmap.bmWidth * 4) * (bmap.bmHeight - 1);

        for (int i = 0; i < bmap.bmHeight; ++i)
        {
            memcpy(dstPtr, srcPtr, bmap.bmWidth * 4);
            srcPtr -= bmap.bmWidth * 4;
            dstPtr += pitch;
        }
    }

    _transparentBitmapSurface->Unlock(NULL);
    return 0;
}
/**
 *
 *   DirectDrawTextSettings
 *
 */
DirectDrawTextSettings::DirectDrawTextSettings() :
    _ptrText(NULL),
    _textLength(0),
    _colorRefText(RGB(255, 255, 255)), // white
    _colorRefBackground(RGB(0, 0, 0)), // black
    _textLeft(0.0f),
    _textRight(0.0f),
    _textTop(0.0f),
    _textBottom(0.0f),
    _transparent(true)
{
}

DirectDrawTextSettings::~DirectDrawTextSettings()
{
    if (_ptrText)
    {
        delete[] _ptrText;
    }
}

int DirectDrawTextSettings::SetText(const char* text, int textLength,
                                        COLORREF colorText, COLORREF colorBg,
                                        float left, float top, float right,
                                        float bottom)
{
    if (_ptrText)
    {
        delete[] _ptrText;
    }
    _ptrText = new char[textLength];
    memcpy(_ptrText, text, textLength);
    _textLength = textLength;
    _colorRefText = colorText;
    _colorRefBackground = colorBg;
    //_transparent = transparent;
    _textLeft = left;
    _textRight = right;
    _textTop = top;
    _textBottom = bottom;
    return 0;
}

/**
 *
 *	DirectDrawChannel
 *
 *
 */

// this need to have a refcount dueto multiple HWNDS demux
DirectDrawChannel::DirectDrawChannel(DirectDraw* directDraw,
                                             VideoType blitVideoType,
                                             VideoType incomingVideoType,
                                             VideoType screenVideoType,
                                             VideoRenderDirectDraw* owner) :

    _critSect(CriticalSectionWrapper::CreateCriticalSection()), _refCount(1),
            _width(0), _height(0), _numberOfStreams(0), _doubleBuffer(false),
            _directDraw(directDraw), _offScreenSurface(NULL),
            _offScreenSurfaceNext(NULL), _incomingVideoType(incomingVideoType),
            _blitVideoType(blitVideoType),
            _originalBlitVideoType(blitVideoType),
            _screenVideoType(screenVideoType), _deliverInScreenType(false),
            _owner(owner)
{
    _directDraw->AddRef();
}

DirectDrawChannel::~DirectDrawChannel()
{
    if (_directDraw)
    {
        _directDraw->Release();
    }
    if (_offScreenSurface)
    {
        _offScreenSurface->Release();
    }
    if (_offScreenSurfaceNext)
    {
        _offScreenSurfaceNext->Release();
    }
    std::map<unsigned long long, DirectDrawStreamSettings*>::iterator it =
            _streamIdToSettings.begin();
    while (it != _streamIdToSettings.end())
    {
        DirectDrawStreamSettings* streamSettings = it->second;
        if (streamSettings)
        {
            delete streamSettings;
        }
        it = _streamIdToSettings.erase(it);
    }
    delete _critSect;
}

void DirectDrawChannel::AddRef()
{
    CriticalSectionScoped cs(_critSect);
    _refCount++;
}

void DirectDrawChannel::Release()
{
    bool deleteObj = false;
    _critSect->Enter();
    _refCount--;
    if (_refCount == 0)
    {
        deleteObj = true;
    }
    _critSect->Leave();

    if (deleteObj)
    {
        delete this;
    }
}

void DirectDrawChannel::SetStreamSettings(VideoRenderDirectDraw* DDobj,
                                              short streamId, float startWidth,
                                              float startHeight,
                                              float stopWidth, float stopHeight)
{
    // we can save 5 bits due to 16 byte alignment of the pointer
    unsigned long long lookupID = reinterpret_cast<unsigned long long> (DDobj);
    lookupID &= 0xffffffffffffffe0;
    lookupID <<= 11;
    lookupID += streamId;

    CriticalSectionScoped cs(_critSect);

    DirectDrawStreamSettings* streamSettings = NULL;

    std::map<unsigned long long, DirectDrawStreamSettings*>::iterator it =
            _streamIdToSettings.find(lookupID);
    if (it == _streamIdToSettings.end())
    {
        streamSettings = new DirectDrawStreamSettings();
        _streamIdToSettings[lookupID] = streamSettings;
    }
    else
    {
        streamSettings = it->second;
    }

    streamSettings->_startHeight = startHeight;
    streamSettings->_startWidth = startWidth;
    streamSettings->_stopWidth = stopWidth;
    streamSettings->_stopHeight = stopHeight;

    _offScreenSurfaceUpdated = false;
}

void DirectDrawChannel::SetStreamCropSettings(VideoRenderDirectDraw* DDObj,
                                                  short streamId,
                                                  float startWidth,
                                                  float startHeight,
                                                  float stopWidth,
                                                  float stopHeight)
{
    unsigned long long lookupID = reinterpret_cast<unsigned long long> (DDObj);
    lookupID &= 0xffffffffffffffe0;
    lookupID <<= 11;
    lookupID += streamId;

    CriticalSectionScoped cs(_critSect);

    DirectDrawStreamSettings* streamSettings = NULL;
    std::map<unsigned long long, DirectDrawStreamSettings*>::iterator it =
            _streamIdToSettings.find(lookupID);
    if (it == _streamIdToSettings.end())
    {
        streamSettings = new DirectDrawStreamSettings();
        _streamIdToSettings[streamId] = streamSettings;
    }
    else
    {
        streamSettings = it->second;
    }
    streamSettings->_cropStartWidth = startWidth;
    streamSettings->_cropStopWidth = stopWidth;
    streamSettings->_cropStartHeight = startHeight;
    streamSettings->_cropStopHeight = stopHeight;
}

int DirectDrawChannel::GetStreamSettings(VideoRenderDirectDraw* DDObj,
                                             short streamId, float& startWidth,
                                             float& startHeight,
                                             float& stopWidth,
                                             float& stopHeight)
{
    CriticalSectionScoped cs(_critSect);

    unsigned long long lookupID = reinterpret_cast<unsigned long long> (DDObj);
    lookupID &= 0xffffffffffffffe0;
    lookupID <<= 11;
    lookupID += streamId;

    DirectDrawStreamSettings* streamSettings = NULL;
    std::map<unsigned long long, DirectDrawStreamSettings*>::iterator it =
            _streamIdToSettings.find(lookupID);
    if (it == _streamIdToSettings.end())
    {
        // Didn't find this stream...
        return -1;
    }
    streamSettings = it->second;
    startWidth = streamSettings->_startWidth;
    startHeight = streamSettings->_startHeight;
    stopWidth = streamSettings->_stopWidth;
    stopHeight = streamSettings->_stopHeight;

    return 0;
}

bool DirectDrawChannel::IsOffScreenSurfaceUpdated(VideoRenderDirectDraw* DDobj)
{
    CriticalSectionScoped cs(_critSect);
    return _offScreenSurfaceUpdated;
}

void DirectDrawChannel::GetLargestSize(RECT* mixingRect)
{
    CriticalSectionScoped cs(_critSect);
    if (mixingRect)
    {
        if (mixingRect->bottom < _height)
        {
            mixingRect->bottom = _height;
        }
        if (mixingRect->right < _width)
        {
            mixingRect->right = _width;
        }
    }
}

int DirectDrawChannel::ChangeDeliverColorFormat(bool useScreenType)
{
    _deliverInScreenType = useScreenType;
    return FrameSizeChange(0, 0, 0);
}

WebRtc_Word32 DirectDrawChannel::RenderFrame(const WebRtc_UWord32 streamId,
                                                 VideoFrame& videoFrame)
{
    CriticalSectionScoped cs(_critSect);
    if (_width != videoFrame.Width() || _height != videoFrame.Height())
    {
        if (FrameSizeChange(videoFrame.Width(), videoFrame.Height(), 1) == -1)
        {
            return -1;
        }
    }
    return DeliverFrame(videoFrame.Buffer(), videoFrame.Length(),
                        videoFrame.TimeStamp());
}

int DirectDrawChannel::FrameSizeChange(int width, int height,
                                           int numberOfStreams)
{
    CriticalSectionScoped cs(_critSect);

    if (_directDraw == NULL)
    {
        return -1; // signal that we are not ready for the change
    }
    if (_width == width && _height == height && _offScreenSurface
            && _offScreenSurfaceNext)
    {
        _numberOfStreams = numberOfStreams;
        return 0;
    }
    if (_offScreenSurface)
    {
        _offScreenSurface->Release();
        _offScreenSurface = NULL;
    }
    if (_offScreenSurfaceNext)
    {
        _offScreenSurfaceNext->Release();
        _offScreenSurfaceNext = NULL;
    }
    if (width && height)
    {
        _width = width;
        _height = height;
        _numberOfStreams = numberOfStreams;
    }

    // create this channels offscreen buffer
    DirectDrawSurfaceDesc ddsd;
    HRESULT ddrval = DD_OK;
    memset(&ddsd, 0, sizeof(ddsd));
    ddsd.dwSize = sizeof(ddsd);
    ddsd.dwFlags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH | DDSD_PIXELFORMAT;
    ddsd.ddsCaps.dwCaps = DDSCAPS_VIDEOMEMORY;
    ddsd.dwHeight = _height;
    ddsd.dwWidth = _width;
    /*
     char logStr[256];
     _snprintf(logStr,256, "offscreen H:%d W:%d \n",_height, _width);
     OutputDebugString(logStr);
     */
    //Fix for bad video driver on HP Mini. If it takes to long time to deliver a frame - try to blit using the same pixel format as used by the screen.
    if (_deliverInScreenType && _screenVideoType != kUnknown)
    {
        //The HP mini netbook, which this fix for, uses the VIA processor.
        //The measuring shows that this fix will impact systems with Intel processor, including Atom.
        //So let's disable it here. If we really need this for VIA processor, we should have additional logic to detect
        //the processor model.
        //WEBRTC_TRACE(kTraceInfo, kTraceVideo, -1, "DirectDrawChannel changing to screen video type");
        //_blitVideoType=_screenVideoType;
    }
    else
    {
        WEBRTC_TRACE(
                     kTraceInfo,
                     kTraceVideo,
                     -1,
                     "DirectDrawChannel changing to originial blit video type %d",
                     _originalBlitVideoType);
        _blitVideoType = _originalBlitVideoType;
    }

    WEBRTC_TRACE(
                 kTraceInfo,
                 kTraceVideo,
                 -1,
                 "DirectDrawChannel::FrameSizeChange height %d, width %d, _blitVideoType %d",
                 ddsd.dwHeight, ddsd.dwWidth, _blitVideoType);
    switch (_blitVideoType)
    {
        case kYV12:
        {
            ddsd.ddpfPixelFormat.dwSize = sizeof(DDPIXELFORMAT);
            ddsd.ddpfPixelFormat.dwFlags = DDPF_FOURCC;
            ddsd.ddpfPixelFormat.dwFourCC = MAKEFOURCC('Y', 'V', '1', '2');
        }
            break;
        case kYUY2:
        {
            ddsd.ddpfPixelFormat.dwSize = sizeof(DDPIXELFORMAT);
            ddsd.ddpfPixelFormat.dwFlags = DDPF_FOURCC;
            ddsd.ddpfPixelFormat.dwFourCC = MAKEFOURCC('Y', 'U', 'Y', '2');
        }
            break;
        case kUYVY:
        {
            ddsd.ddpfPixelFormat.dwSize = sizeof(DDPIXELFORMAT);
            ddsd.ddpfPixelFormat.dwFlags = DDPF_FOURCC;
            ddsd.ddpfPixelFormat.dwFourCC = MAKEFOURCC('U', 'Y', 'V', 'Y');
        }
            break;
        case kIYUV:
        {
            ddsd.ddpfPixelFormat.dwSize = sizeof(DDPIXELFORMAT);
            ddsd.ddpfPixelFormat.dwFlags = DDPF_FOURCC;
            ddsd.ddpfPixelFormat.dwFourCC = MAKEFOURCC('I', 'Y', 'U', 'V');
        }
            break;
        case kARGB:
        {
            ddsd.ddpfPixelFormat.dwSize = sizeof(DDPIXELFORMAT);
            ddsd.ddpfPixelFormat.dwFlags = DDPF_RGB;
            ddsd.ddpfPixelFormat.dwRGBBitCount = 32;
            ddsd.ddpfPixelFormat.dwRBitMask = 0xff0000;
            ddsd.ddpfPixelFormat.dwGBitMask = 0xff00;
            ddsd.ddpfPixelFormat.dwBBitMask = 0xff;
            ddsd.ddpfPixelFormat.dwRGBAlphaBitMask = 0;
        }
            break;
        case kRGB24:
        {
            ddsd.ddpfPixelFormat.dwSize = sizeof(DDPIXELFORMAT);
            ddsd.ddpfPixelFormat.dwFlags = DDPF_RGB;
            ddsd.ddpfPixelFormat.dwRGBBitCount = 24;
            ddsd.ddpfPixelFormat.dwRBitMask = 0xff0000;
            ddsd.ddpfPixelFormat.dwGBitMask = 0xff00;
            ddsd.ddpfPixelFormat.dwBBitMask = 0xff;
            ddsd.ddpfPixelFormat.dwRGBAlphaBitMask = 0;
        }
            break;
        case kRGB565:
        {
            ddsd.ddpfPixelFormat.dwSize = sizeof(DDPIXELFORMAT);
            ddsd.ddpfPixelFormat.dwFlags = DDPF_RGB;
            ddsd.ddpfPixelFormat.dwRGBBitCount = 16;
            ddsd.ddpfPixelFormat.dwRBitMask = 0x0000F800;
            ddsd.ddpfPixelFormat.dwGBitMask = 0x000007e0;
            ddsd.ddpfPixelFormat.dwBBitMask = 0x0000001F;
            ddsd.ddpfPixelFormat.dwRGBAlphaBitMask = 0;
        }
            break;
        case kARGB4444:
        {
            ddsd.ddpfPixelFormat.dwSize = sizeof(DDPIXELFORMAT);
            ddsd.ddpfPixelFormat.dwFlags = DDPF_RGB;
            ddsd.ddpfPixelFormat.dwRGBBitCount = 16;
            ddsd.ddpfPixelFormat.dwRBitMask = 0x00000f00;
            ddsd.ddpfPixelFormat.dwGBitMask = 0x000000f0;
            ddsd.ddpfPixelFormat.dwBBitMask = 0x0000000f;
            ddsd.ddpfPixelFormat.dwRGBAlphaBitMask = 0;
            break;
        }
        case kARGB1555:
        {
            ddsd.ddpfPixelFormat.dwSize = sizeof(DDPIXELFORMAT);
            ddsd.ddpfPixelFormat.dwFlags = DDPF_RGB;
            ddsd.ddpfPixelFormat.dwRGBBitCount = 16;
            ddsd.ddpfPixelFormat.dwRBitMask = 0x00007C00;
            ddsd.ddpfPixelFormat.dwGBitMask = 0x3E0;
            ddsd.ddpfPixelFormat.dwBBitMask = 0x1F;
            ddsd.ddpfPixelFormat.dwRGBAlphaBitMask = 0;
            break;
        }
        case kI420:
        {
            ddsd.ddpfPixelFormat.dwSize = sizeof(DDPIXELFORMAT);
            ddsd.ddpfPixelFormat.dwFlags = DDPF_FOURCC;
            ddsd.ddpfPixelFormat.dwFourCC = MAKEFOURCC('I', '4', '2', '0');
        }
            break;
        default:
            ddrval = S_FALSE;
    }

    if (ddrval == DD_OK)
    {
        if (!_owner->IsPrimaryOrMixingSurfaceOnSystem())
        {
            ddrval
                    = _directDraw->CreateSurface(&ddsd, &_offScreenSurface,
                                                 NULL);
            if (FAILED(ddrval))
            {
                WEBRTC_TRACE(
                             kTraceInfo,
                             kTraceVideo,
                             -1,
                             "CreateSurface failed for _offScreenSurface on VideoMemory, trying on System Memory");

                memset(&ddsd, 0, sizeof(ddsd));
                ddsd.dwSize = sizeof(ddsd);
                ddsd.dwFlags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH;

                ddsd.dwHeight = _height;
                ddsd.dwWidth = _width;

                ddsd.ddsCaps.dwCaps = DDSCAPS_SYSTEMMEMORY;
                _blitVideoType = kARGB;

                ddrval = _directDraw->CreateSurface(&ddsd, &_offScreenSurface,
                                                    NULL);
                if (FAILED(ddrval))
                {
                    WEBRTC_TRACE(
                                 kTraceError,
                                 kTraceVideo,
                                 -1,
                                 "DirectDraw failed to CreateSurface _offScreenSurface using SystemMemory: 0x%x",
                                 ddrval);
                }
                ddrval = _directDraw->CreateSurface(&ddsd,
                                                    &_offScreenSurfaceNext,
                                                    NULL);
                if (FAILED(ddrval))
                {
                    WEBRTC_TRACE(
                                 kTraceError,
                                 kTraceVideo,
                                 -1,
                                 "DirectDraw failed to CreateSurface _offScreenSurfaceNext using SystemMemory: 0x%x",
                                 ddrval);
                }
            }
            else
            {
                ddrval = _directDraw->CreateSurface(&ddsd,
                                                    &_offScreenSurfaceNext,
                                                    NULL);
                if (ddrval == DDERR_OUTOFVIDEOMEMORY)
                {
                    WEBRTC_TRACE(
                                 kTraceInfo,
                                 kTraceVideo,
                                 -1,
                                 "CreateSurface failed for _offScreenSurfaceNext on VideoMemory, trying on System Memory");

                    memset(&ddsd, 0, sizeof(ddsd));
                    ddsd.dwSize = sizeof(ddsd);
                    ddsd.dwFlags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH;

                    ddsd.dwHeight = _height;
                    ddsd.dwWidth = _width;

                    ddsd.ddsCaps.dwCaps = DDSCAPS_SYSTEMMEMORY;
                    _blitVideoType = kARGB;

                    ddrval = _directDraw->CreateSurface(&ddsd,
                                                        &_offScreenSurfaceNext,
                                                        NULL);
                    if (FAILED(ddrval))
                    {
                        WEBRTC_TRACE(
                                     kTraceError,
                                     kTraceVideo,
                                     -1,
                                     "DirectDraw failed to CreateSurface _offScreenSurfaceNext using SystemMemory: 0x%x",
                                     ddrval);
                    }
                }
            }
        }
        else
        {
            memset(&ddsd, 0, sizeof(ddsd));
            ddsd.dwSize = sizeof(ddsd);
            ddsd.dwFlags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH;

            ddsd.dwHeight = _height;
            ddsd.dwWidth = _width;

            ddsd.ddsCaps.dwCaps = DDSCAPS_SYSTEMMEMORY;
            if (_owner->CanBltFourCC())
            {
                _blitVideoType = kARGB;
            }
            else
            {
                _blitVideoType = _originalBlitVideoType;
            }

            ddrval
                    = _directDraw->CreateSurface(&ddsd, &_offScreenSurface,
                                                 NULL);
            if (FAILED(ddrval))
            {
                WEBRTC_TRACE(
                             kTraceError,
                             kTraceVideo,
                             -1,
                             "DirectDraw failed to CreateSurface _offScreenSurface using SystemMemory: 0x%x",
                             ddrval);
            }

            ddrval = _directDraw->CreateSurface(&ddsd, &_offScreenSurfaceNext,
                                                NULL);
            if (FAILED(ddrval))
            {
                WEBRTC_TRACE(
                             kTraceError,
                             kTraceVideo,
                             -1,
                             "DirectDraw failed to CreateSurface _offScreenSurfaceNext using SystemMemory: 0x%x",
                             ddrval);
            }
        }
    }

    if (FAILED(ddrval))
    {
        // failed to change size
        WEBRTC_TRACE(kTraceError, kTraceVideo, -1,
                     "DirectDraw failed to CreateSurface : 0x%x", ddrval);
        return -1;
    }

    return 0;
}

int DirectDrawChannel::DeliverFrame(unsigned char* buffer, int bufferSize,
                                        unsigned int /*timeStamp90KHz*/)
{
    CriticalSectionScoped cs(_critSect);

    if (CalcBufferSize(_incomingVideoType, _width, _height)
            != bufferSize)
    {
        // sanity
        return -1;
    }
    if (!_offScreenSurface || !_offScreenSurfaceNext)
    {
        if (_width && _height && _numberOfStreams)
        {
            // our surface was lost recreate it
            FrameSizeChange(_width, _height, _numberOfStreams);
        }
        return -1;
    }
    if (_offScreenSurface->IsLost() == DDERR_SURFACELOST)
    {
        HRESULT ddrval = _offScreenSurface->Restore();
        if (ddrval != DD_OK)
        {
            // failed to restore our surface remove it and it will be re-created in next frame
            _offScreenSurface->Release();
            _offScreenSurface = NULL;
            _offScreenSurfaceNext->Release();
            _offScreenSurfaceNext = NULL;
            return -1;
        }
        ddrval = _offScreenSurfaceNext->Restore();
        if (ddrval != DD_OK)
        {
            // failed to restore our surface remove it and it will be re-created in next frame
            _offScreenSurface->Release();
            _offScreenSurface = NULL;
            _offScreenSurfaceNext->Release();
            _offScreenSurfaceNext = NULL;
            return -1;
        }
    }
    _doubleBuffer = false;

    // check if _offScreenSurfaceUpdated is true
    DirectDrawSurface* offScreenSurface = _offScreenSurface;
    {

        if (_offScreenSurfaceUpdated)
        {
            // this frame is not yet rendered
            offScreenSurface = _offScreenSurfaceNext;
            _doubleBuffer = true;
        }
    }

    DirectDrawSurfaceDesc ddsd;
    memset(&ddsd, 0, sizeof(ddsd));
    ddsd.dwSize = sizeof(ddsd);
    HRESULT ddrval = offScreenSurface->Lock(NULL, &ddsd, DDLOCK_WAIT, NULL);
    if (ddrval == DDERR_SURFACELOST)
    {
        WEBRTC_TRACE(kTraceInfo, kTraceVideo, -1,
                     "DirectDrawChannel::DeliverFrame offScreenSurface lost");
        ddrval = offScreenSurface->Restore();
        if (ddrval != DD_OK)
        {
            // failed to restore our surface remove it and it will be re-created in next frame
            _offScreenSurface->Release();
            _offScreenSurface = NULL;
            _offScreenSurfaceNext->Release();
            _offScreenSurfaceNext = NULL;
            return -1;
        }
        return 0;
    }
    if (ddrval != DD_OK)
    {
        WEBRTC_TRACE(kTraceInfo, kTraceVideo, -1,
                     "DirectDrawChannel::DeliverFrame failed to lock");
        // failed to lock our surface remove it and it will be re-created in next frame
        _offScreenSurface->Release();
        _offScreenSurface = NULL;
        _offScreenSurfaceNext->Release();
        _offScreenSurfaceNext = NULL;
        return -1;
    }

    unsigned char* ptr = (unsigned char*) ddsd.lpSurface;
    // ddsd.lPitch; distance in bytes


    switch (_incomingVideoType)
    {
        case kI420:
        {
            switch (_blitVideoType)
            {
                case kYUY2:
                case kUYVY:
                case kIYUV:  // same as kYV12
                case kYV12:
                    ConvertFromI420(buffer, _width,
                                    _blitVideoType, 0,
                                    _width, _height,
                                    ptr);
                    break;
                case kRGB24:
                {
                    _tempRenderBuffer.VerifyAndAllocate(_width * _height * 3);
                    unsigned char *ptrTempBuffer = _tempRenderBuffer.Buffer();
                    ConvertFromI420(buffer, _width, kRGB24, 0, _width, _height,
                                    ptrTempBuffer);
                    for (int i = 0; i < _height; i++)
                    {
                        memcpy(ptr, ptrTempBuffer, _width * 3);
                        ptrTempBuffer += _width * 3;
                        ptr += ddsd.lPitch;
                    }
                    break;
                }
                case kARGB:
                  ConvertFromI420(buffer, ddsd.lPitch, kARGB, 0,
                                  _width, _height, ptr);
                    break;
                case kARGB4444:
                    ConvertI420ToARGB4444(buffer, ptr, _width, _height,
                                          (ddsd.lPitch >> 1) - _width);
                    break;
                case kARGB1555:
                    ConvertI420ToARGB1555(buffer, ptr, _width, _height,
                                          (ddsd.lPitch >> 1) - _width);
                    break;
                case kRGB565:
                {
                    _tempRenderBuffer.VerifyAndAllocate(_width * _height * 2);
                    unsigned char *ptrTempBuffer = _tempRenderBuffer.Buffer();
                    ConvertI420ToRGB565(buffer, ptrTempBuffer, _width, _height);
                    ptr += ddsd.lPitch * (_height - 1);
                    for (int i = 0; i < _height; i++)
                    {
                        memcpy(ptr, ptrTempBuffer, _width * 2);
                        ptrTempBuffer += _width * 2;
                        ptr -= ddsd.lPitch;
                    }
                    break;
                }
                default:
                  assert(false &&
                      "DirectDrawChannel::DeliverFrame unknown blitVideoType");
                  WEBRTC_TRACE(kTraceError, kTraceVideo, -1,
                               "%s unknown blitVideoType %d",
                               __FUNCTION__, _blitVideoType);
            }
            break;
        }
        default:
            assert(false &&
                "DirectDrawChannel::DeliverFrame wrong incomming video type");
            WEBRTC_TRACE(kTraceError, kTraceVideo, -1,
                         "%s wrong incomming video type:%d",
                         __FUNCTION__, _incomingVideoType);
    }
    _offScreenSurfaceUpdated = true;
    offScreenSurface->Unlock(NULL);
    return 0;
}

int DirectDrawChannel::BlitFromOffscreenBufferToMixingBuffer(
                                                                 VideoRenderDirectDraw* DDobj,
                                                                 short streamID,
                                                                 DirectDrawSurface* mixingSurface,
                                                                 RECT &hwndRect,
                                                                 bool demuxing)
{
    HRESULT ddrval;
    RECT srcRect;
    RECT dstRect;
    DirectDrawStreamSettings* streamSettings = NULL;
    unsigned long long lookupID = reinterpret_cast<unsigned long long> (DDobj);
    lookupID &= 0xffffffffffffffe0;
    lookupID <<= 11;
    lookupID += streamID;

    CriticalSectionScoped cs(_critSect);

    if (_offScreenSurface == NULL)
    {
        // The offscreen surface has been deleted but not restored yet
        return 0;
    }
    if (mixingSurface == NULL)
    {
        // Not a valid input argument
        return 0;
    }

    std::map<unsigned long long, DirectDrawStreamSettings*>::iterator it =
            _streamIdToSettings.find(lookupID);
    if (it == _streamIdToSettings.end())
    {
        // ignore this stream id
        return 0;
    }
    streamSettings = it->second;

    int numberOfStreams = _numberOfStreams;
    if (!demuxing)
    {
        numberOfStreams = 1; // treat as one stream if we only have one config
    }

    switch (numberOfStreams)
    {
        case 0:
            return 0;
        case 1:
        {
            // no demux
            if (streamID > 0)
                return 0;

            ::SetRect(&srcRect, int(_width * streamSettings->_cropStartWidth),
                      int(_height * streamSettings->_cropStartHeight),
                      int(_width * streamSettings->_cropStopWidth), int(_height
                              * streamSettings->_cropStopHeight));

            ::SetRect(&dstRect, int(hwndRect.right
                    * streamSettings->_startWidth), int(hwndRect.bottom
                    * streamSettings->_startHeight), int(hwndRect.right
                    * streamSettings->_stopWidth), int(hwndRect.bottom
                    * streamSettings->_stopHeight));
        }
            break;
        case 2:
        case 3:
        case 4:
            // classic quadrant demux
        {
            int width = _width >> 1;
            int height = _height >> 1;
            ::SetRect(&srcRect, int(width * streamSettings->_cropStartWidth),
                      int(height * streamSettings->_cropStartHeight), int(width
                              * streamSettings->_cropStopWidth), int(height
                              * streamSettings->_cropStopHeight));

            ::SetRect(&dstRect, int(hwndRect.right
                    * streamSettings->_startWidth), int(hwndRect.bottom
                    * streamSettings->_startHeight), int(hwndRect.right
                    * streamSettings->_stopWidth), int(hwndRect.bottom
                    * streamSettings->_stopHeight));

            // stream id to select quadrant
            if (streamID == 1)
            {
                ::OffsetRect(&srcRect, width, 0);
            }
            if (streamID == 2)
            {
                ::OffsetRect(&srcRect, 0, height);
            }
            if (streamID == 3)
            {
                ::OffsetRect(&srcRect, width, height);
            }
        }
            break;
        case 5:
        case 6:
        {
            const int width = (_width / (3 * 16)) * 16;
            const int widthMidCol = width + ((_width % (16 * 3)) / 16) * 16;
            const int height = _height / (2 * 16) * 16;
            if (streamID == 1 || streamID == 4)
            {
                ::SetRect(&srcRect, int(widthMidCol
                        * streamSettings->_cropStartWidth), int(height
                        * streamSettings->_cropStartHeight), int(widthMidCol
                        * streamSettings->_cropStopWidth), int(height
                        * streamSettings->_cropStopHeight));
            }
            else
            {
                ::SetRect(&srcRect,
                          int(width * streamSettings->_cropStartWidth),
                          int(height * streamSettings->_cropStartHeight),
                          int(width * streamSettings->_cropStopWidth),
                          int(height * streamSettings->_cropStopHeight));
            }
            ::SetRect(&dstRect, int(hwndRect.right
                    * streamSettings->_startWidth), int(hwndRect.bottom
                    * streamSettings->_startHeight), int(hwndRect.right
                    * streamSettings->_stopWidth), int(hwndRect.bottom
                    * streamSettings->_stopHeight));

            // stream id to select quadrant
            switch (streamID)
            {
                case 1:
                    ::OffsetRect(&srcRect, width, 0);
                    break;
                case 2:
                    ::OffsetRect(&srcRect, width + widthMidCol, 0);
                    break;
                case 3:
                    ::OffsetRect(&srcRect, 0, height);
                    break;
                case 4:
                    ::OffsetRect(&srcRect, width, height);
                    break;
                case 5:
                    ::OffsetRect(&srcRect, width + widthMidCol, height);
                    break;
            }
        }
            break;
        case 7:
        case 8:
        case 9:

        {
            const int width = (_width / (3 * 16)) * 16;
            const int widthMidCol = width + ((_width % (16 * 3)) / 16) * 16;
            const int height = _height / (3 * 16) * 16;
            const int heightMidRow = height + ((_height % (16 * 3)) / 16) * 16;

            ::SetRect(&dstRect, int(hwndRect.right
                    * streamSettings->_startWidth), int(hwndRect.bottom
                    * streamSettings->_startHeight), int(hwndRect.right
                    * streamSettings->_stopWidth), int(hwndRect.bottom
                    * streamSettings->_stopHeight));

            switch (streamID)
            {
                case 0:
                    //Size
                    ::SetRect(&srcRect, int(width
                            * streamSettings->_cropStartWidth), int(height
                            * streamSettings->_cropStartHeight), int(width
                            * streamSettings->_cropStopWidth), int(height
                            * streamSettings->_cropStopHeight));
                    //Position
                    ::OffsetRect(&srcRect, 0, 0);
                    break;
                case 1:
                    ::SetRect(
                              &srcRect,
                              int(widthMidCol * streamSettings->_cropStartWidth),
                              int(height * streamSettings->_cropStartHeight),
                              int(widthMidCol * streamSettings->_cropStopWidth),
                              int(height * streamSettings->_cropStopHeight));
                    ::OffsetRect(&srcRect, width, 0);
                    break;
                case 2:
                    ::SetRect(&srcRect, int(width
                            * streamSettings->_cropStartWidth), int(height
                            * streamSettings->_cropStartHeight), int(width
                            * streamSettings->_cropStopWidth), int(height
                            * streamSettings->_cropStopHeight));
                    ::OffsetRect(&srcRect, width + widthMidCol, 0);
                    break;
                case 3:
                    ::SetRect(&srcRect, int(width
                            * streamSettings->_cropStartWidth),
                              int(heightMidRow
                                      * streamSettings->_cropStartHeight),
                              int(width * streamSettings->_cropStopWidth),
                              int(heightMidRow
                                      * streamSettings->_cropStopHeight));
                    ::OffsetRect(&srcRect, 0, height);
                    break;
                case 4:
                    ::SetRect(
                              &srcRect,
                              int(widthMidCol * streamSettings->_cropStartWidth),
                              int(heightMidRow
                                      * streamSettings->_cropStartHeight),
                              int(widthMidCol * streamSettings->_cropStopWidth),
                              int(heightMidRow
                                      * streamSettings->_cropStopHeight));
                    ::OffsetRect(&srcRect, width, height);

                    break;
                case 5:
                    ::SetRect(&srcRect, int(width
                            * streamSettings->_cropStartWidth),
                              int(heightMidRow
                                      * streamSettings->_cropStartHeight),
                              int(width * streamSettings->_cropStopWidth),
                              int(heightMidRow
                                      * streamSettings->_cropStopHeight));
                    ::OffsetRect(&srcRect, width + widthMidCol, height);
                    break;
                case 6:
                    ::SetRect(&srcRect, int(width
                            * streamSettings->_cropStartWidth), int(height
                            * streamSettings->_cropStartHeight), int(width
                            * streamSettings->_cropStopWidth), int(height
                            * streamSettings->_cropStopHeight));
                    ::OffsetRect(&srcRect, 0, height + heightMidRow);
                    break;
                case 7:
                    ::SetRect(
                              &srcRect,
                              int(widthMidCol * streamSettings->_cropStartWidth),
                              int(height * streamSettings->_cropStartHeight),
                              int(widthMidCol * streamSettings->_cropStopWidth),
                              int(height * streamSettings->_cropStopHeight));
                    ::OffsetRect(&srcRect, width, height + heightMidRow);
                    break;
                case 8:
                    ::SetRect(&srcRect, int(width
                            * streamSettings->_cropStartWidth), int(height
                            * streamSettings->_cropStartHeight), int(width
                            * streamSettings->_cropStopWidth), int(height
                            * streamSettings->_cropStopHeight));
                    ::OffsetRect(&srcRect, width + widthMidCol, height
                            + heightMidRow);
                    break;
            }
        }
            break;
        case 10:
        case 11:
        case 12:
        case 13:
        case 14:
        case 15:
        case 16:
        default:
        {
            ::SetRect(&srcRect, int(_width * streamSettings->_cropStartWidth),
                      int(_height * streamSettings->_cropStartHeight),
                      int(_width * streamSettings->_cropStopWidth), int(_height
                              * streamSettings->_cropStopHeight));

            ::SetRect(&dstRect, int(hwndRect.right
                    * streamSettings->_startWidth), int(hwndRect.bottom
                    * streamSettings->_startHeight), int(hwndRect.right
                    * streamSettings->_stopWidth), int(hwndRect.bottom
                    * streamSettings->_stopHeight));
        }
    }

    if (dstRect.right > hwndRect.right)
    {
        srcRect.right -= (int) ((float) (srcRect.right - srcRect.left)
                * ((float) (dstRect.right - hwndRect.right)
                        / (float) (dstRect.right - dstRect.left)));
        dstRect.right = hwndRect.right;
    }
    if (dstRect.left < hwndRect.left)
    {
        srcRect.left += (int) ((float) (srcRect.right - srcRect.left)
                * ((float) (hwndRect.left - dstRect.left)
                        / (float) (dstRect.right - dstRect.left)));
        dstRect.left = hwndRect.left;
    }
    if (dstRect.bottom > hwndRect.bottom)
    {
        srcRect.bottom -= (int) ((float) (srcRect.bottom - srcRect.top)
                * ((float) (dstRect.bottom - hwndRect.bottom)
                        / (float) (dstRect.bottom - dstRect.top)));
        dstRect.bottom = hwndRect.bottom;
    }
    if (dstRect.top < hwndRect.top)
    {
        srcRect.top += (int) ((float) (srcRect.bottom - srcRect.top)
                * ((float) (hwndRect.top - dstRect.top)
                        / (float) (dstRect.bottom - dstRect.top)));
        dstRect.top = hwndRect.top;
    }

    DDBLTFX ddbltfx;
    ZeroMemory(&ddbltfx, sizeof(ddbltfx));
    ddbltfx.dwSize = sizeof(ddbltfx);
    ddbltfx.dwDDFX = DDBLTFX_NOTEARING;

    // wait for the _mixingSurface to be available
    ddrval = mixingSurface->Blt(&dstRect, _offScreenSurface, &srcRect,
                                DDBLT_WAIT | DDBLT_DDFX, &ddbltfx);
    if (ddrval == DDERR_SURFACELOST)
    {
        WEBRTC_TRACE(kTraceInfo, kTraceVideo, -1,
                     "mixingSurface->Blt surface lost");
        ddrval = mixingSurface->Restore();
        if (ddrval != DD_OK)
        {
            // we dont own the surface just report the error
            return -1;
        }
    }
    else if (ddrval == DDERR_INVALIDRECT)
    {
        WEBRTC_TRACE(kTraceInfo, kTraceVideo, -1,
                     "mixingSurface->Blt DDERR_INVALIDRECT");
        WEBRTC_TRACE(
                     kTraceError,
                     kTraceVideo,
                     -1,
                     "dstRect co-ordinates - top: %d left: %d bottom: %d right: %d",
                     dstRect.top, dstRect.left, dstRect.bottom, dstRect.right);
        WEBRTC_TRACE(
                     kTraceError,
                     kTraceVideo,
                     -1,
                     "srcRect co-ordinates - top: %d left: %d bottom: %d right: %d",
                     srcRect.top, srcRect.left, srcRect.bottom, srcRect.right);

        // ignore
    }
    else if (ddrval != DD_OK)
    {
        WEBRTC_TRACE(kTraceInfo, kTraceVideo, -1,
                     "mixingSurface->Blt !DD_OK");
        WEBRTC_TRACE(
                     kTraceError,
                     kTraceVideo,
                     -1,
                     "DirectDraw blt mixingSurface BlitFromOffscreenBufferToMixingBuffer error 0x%x  ",
                     ddrval);

        //logging the co-ordinates and hwnd
        WEBRTC_TRACE(
                     kTraceError,
                     kTraceVideo,
                     -1,
                     "dstRect co-ordinates - top: %d left: %d bottom: %d right: %d",
                     dstRect.top, dstRect.left, dstRect.bottom, dstRect.right);
        WEBRTC_TRACE(
                     kTraceError,
                     kTraceVideo,
                     -1,
                     "srcRect co-ordinates - top: %d left: %d bottom: %d right: %d",
                     srcRect.top, srcRect.left, srcRect.bottom, srcRect.right);

        /*      char logStr[256];
         _snprintf(logStr,256, "srcRect T:%d L:%d B:%d R:%d\n",srcRect.top, srcRect.left, srcRect.bottom, srcRect.right);
         OutputDebugString(logStr);
         char logStr1[256];
         _snprintf(logStr1,256, "dstRect T:%d L:%d B:%d R:%d\n",dstRect.top, dstRect.left, dstRect.bottom, dstRect.right);
         OutputDebugString(logStr1);
         char logStr2[256];
         _snprintf(logStr2,256, "error 0x:%x \n",ddrval);
         OutputDebugString(logStr2);
         */
        // we dont own the surface just report the error
        return -1;
    }
    if (_doubleBuffer)
    {
        DirectDrawSurface* oldOffScreenSurface = _offScreenSurface;
        _offScreenSurface = _offScreenSurfaceNext;
        _offScreenSurfaceNext = oldOffScreenSurface;
        _doubleBuffer = false;
    }
    else
    {
        _offScreenSurfaceUpdated = false;
    }
    return 0;
}

/**
 *
 *	VideoRenderDirectDraw
 *
 *
 */

VideoRenderDirectDraw::VideoRenderDirectDraw(Trace* trace,
                                                     HWND hWnd, bool fullscreen) :
            _trace(trace),
            _confCritSect(CriticalSectionWrapper::CreateCriticalSection()),
            _fullscreen(fullscreen),
            _demuxing(false),
            _transparentBackground(false),
            _supportTransparency(false),
            _canStretch(false),
            _canMirrorLeftRight(false),
            _clearMixingSurface(false),
            _deliverInScreenType(false),
            _renderModeWaitForCorrectScanLine(false),
            _deliverInHalfFrameRate(false),
            _deliverInQuarterFrameRate(false),
            _bCanBltFourcc(true),
            _frameChanged(false),
            _processCount(0),
            _hWnd(hWnd),
            _screenRect(),
            _mixingRect(),

            _incomingVideoType(kUnknown),
            _blitVideoType(kUnknown),
            _rgbVideoType(kUnknown),

            _directDraw(NULL),
            _primarySurface(NULL),
            _backSurface(NULL),
            _mixingSurface(NULL),
            _bitmapSettings(),
            _textSettings(),
            _directDrawChannels(),
            _directDrawZorder(),

            _fullScreenWaitEvent(EventWrapper::Create()),
            _screenEvent(EventWrapper::Create()),
            _screenRenderThread(
                                ThreadWrapper::CreateThread(
                                                            RemoteRenderingThreadProc,
                                                            this,
                                                            kRealtimePriority,
                                                            "Video_directdraw_thread")),
            _blit(true), _lastRenderModeCpuUsage(-1), _totalMemory(-1),
            _availableMemory(-1), _systemCPUUsage(0), _maxAllowedRenderTime(0),
            _nrOfTooLongRenderTimes(0),
            _isPrimaryOrMixingSurfaceOnSystem(false)
{
    SetRect(&_screenRect, 0, 0, 0, 0);
    SetRect(&_mixingRect, 0, 0, 0, 0);
    SetRect(&_originalHwndRect, 0, 0, 0, 0);
    ::GetClientRect(_hWnd, &_hwndRect);
}

VideoRenderDirectDraw::~VideoRenderDirectDraw()
{
    ThreadWrapper* temp = _screenRenderThread;
    _screenRenderThread = NULL;
    if (temp)
    {
        temp->SetNotAlive();
        _screenEvent->Set();
        _screenEvent->StopTimer();
        _fullScreenWaitEvent->StopTimer();

        if (temp->Stop())
        {
            delete temp;
        }
    }
    delete _screenEvent;
    delete _fullScreenWaitEvent;

    std::map<int, DirectDrawChannel*>::iterator it;
    it = _directDrawChannels.begin();
    while (it != _directDrawChannels.end())
    {
        it->second->Release();
        it = _directDrawChannels.erase(it);
    }
    if (_primarySurface)
    {
        _primarySurface->Release();
    }
    if (_mixingSurface)
    {
        _mixingSurface->Release();
    }

    std::map<unsigned char, DirectDrawBitmapSettings*>::iterator bitIt;

    bitIt = _bitmapSettings.begin();
    while (_bitmapSettings.end() != bitIt)
    {
        delete bitIt->second;
        bitIt = _bitmapSettings.erase(bitIt);
    }

    std::map<unsigned char, DirectDrawTextSettings*>::iterator textIt;
    textIt = _textSettings.begin();
    while (_textSettings.end() != textIt)
    {
        delete textIt->second;
        textIt = _textSettings.erase(textIt);
    }
    if (_directDraw)
    {
        _directDraw->Release();
        if (_fullscreen)
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
    }
    delete _confCritSect;
}

WebRtc_Word32 VideoRenderDirectDraw::Init()
{
    int retVal = 0;
    HRESULT ddrval = DirectDrawCreateEx(NULL, (void**) &_directDraw,
                                        IID_IDirectDraw7, NULL);
    if (FAILED(ddrval) || NULL == _directDraw)
    {
        WEBRTC_TRACE(kTraceError, kTraceVideo, -1,
                     "Failed to created DirectDraw7 object");
        return -1;
        //return VIDEO_DIRECT_DRAW_FAILURE;
    }
    retVal = CheckCapabilities();
    if (retVal != 0)
    {
        WEBRTC_TRACE(kTraceError, kTraceVideo, -1,
                     "DirectDraw CheckCapabilities failed");
        return retVal;
    }
    if (_hWnd)
    {
        retVal = CreatePrimarySurface();
        if (retVal != 0)
        {
            WEBRTC_TRACE(kTraceError, kTraceVideo, -1,
                         "DirectDraw failed to CreatePrimarySurface");
            return retVal;
        }
        retVal = CreateMixingSurface();
        if (retVal != 0)
        {
            WEBRTC_TRACE(kTraceError, kTraceVideo, -1,
                         "DirectDraw failed to CreateMixingSurface");
            return retVal;
        }
        if (_screenRenderThread)
        {
            unsigned int tid;
            _screenRenderThread->Start(tid);
            WEBRTC_TRACE(kTraceInfo, kTraceVideo, -1,
                         "Screen Render thread started, thread id: %d", tid);
        }
        DWORD freq = 0;
        _directDraw->GetMonitorFrequency(&freq);
        if (freq == 0)
        {
            freq = 60;
        }
        // Do this now to not do it in each render process loop
        _maxAllowedRenderTime = (int) (1000 / freq * 0.8F);
        _nrOfTooLongRenderTimes = 0;

        _screenEvent->StartTimer(true, 1000 / freq);

        _deliverInScreenType = false;
        _renderModeWaitForCorrectScanLine = false;
        _deliverInHalfFrameRate = false;
        _deliverInQuarterFrameRate = false;

        _lastRenderModeCpuUsage = -1;
        if (_fullscreen)
        {
            _fullScreenWaitEvent->StartTimer(true, 1);
        }

        WEBRTC_TRACE(kTraceInfo, kTraceVideo, -1,
                     "Screen freq %d", freq);
    }
    WEBRTC_TRACE(kTraceInfo, kTraceVideo, -1,
                 "Created DirectDraw object");
    return 0;
}

WebRtc_Word32 VideoRenderDirectDraw::GetGraphicsMemory(
                                                           WebRtc_UWord64& totalMemory,
                                                           WebRtc_UWord64& availableMemory)
{
    CriticalSectionScoped cs(_confCritSect);

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

int VideoRenderDirectDraw::GetScreenResolution(int& screenWidth,
                                                   int& screenHeight)
{
    CriticalSectionScoped cs(_confCritSect);

    screenWidth = _screenRect.right - _screenRect.left;
    screenHeight = _screenRect.bottom - _screenRect.top;
    return 0;
}

int VideoRenderDirectDraw::UpdateSystemCPUUsage(int systemCPU)
{
    CriticalSectionScoped cs(_confCritSect);
    if (systemCPU <= 100 && systemCPU >= 0)
    {
        _systemCPUUsage = systemCPU;
    }
    return 0;
}

int VideoRenderDirectDraw::CheckCapabilities()
{
    HRESULT ddrval = DD_OK;
    DDCAPS ddcaps;
    DDCAPS ddcapsEmul;
    memset(&ddcaps, 0, sizeof(ddcaps));
    memset(&ddcapsEmul, 0, sizeof(ddcapsEmul));
    ddcaps.dwSize = sizeof(ddcaps);
    ddcapsEmul.dwSize = sizeof(ddcapsEmul);
    if (_directDraw == NULL)
    {
        WEBRTC_TRACE(kTraceError, kTraceVideo, -1,
                     "DirectDraw object not created");
        return -1;
        //return VIDEO_DIRECT_DRAW_FAILURE;
    }
    if (IsRectEmpty(&_screenRect))
    {
        ::GetWindowRect(GetDesktopWindow(), &_screenRect);
    }
    // Log Screen resolution
    WEBRTC_TRACE(kTraceInfo, kTraceVideo, -1,
                 "ScreenRect. Top: %d, left: %d, bottom: %d, right: %d",
                 _screenRect.top, _screenRect.left, _screenRect.bottom,
                 _screenRect.right);

    bool fullAccelerationEnabled = false;
    bool badDriver = false;
    VideoRenderWindowsImpl::CheckHWDriver(badDriver, fullAccelerationEnabled);
    if (!fullAccelerationEnabled)
    {

        WEBRTC_TRACE(kTraceError, kTraceVideo, -1,
                     "Direct draw Hardware acceleration is not enabled.");
        return -1;
        //return VIDEO_DIRECT_DRAW_HWACC_NOT_ENABLED;

    }

    // ddcaps supported by the HW
    // ddcapsEmul supported by the OS emulating the HW
    ddrval = _directDraw->GetCaps(&ddcaps, &ddcapsEmul);
    if (ddrval != DD_OK)
    {
        WEBRTC_TRACE(kTraceError, kTraceVideo, -1,
                     "DirectDraw HW: could not get capabilities: %x", ddrval);
        return -1;
        //return VIDEO_DIRECT_DRAW_FAILURE;
    }

    unsigned int minVideoMemory = 3 * 4 * (_screenRect.right
            * _screenRect.bottom); // assuming ARGB size (4 bytes)

    // Store the memory for possible calls to GetMemory()
    _totalMemory = ddcaps.dwVidMemTotal;
    _availableMemory = ddcaps.dwVidMemFree;

    if (ddcaps.dwVidMemFree < minVideoMemory)
    {
        WEBRTC_TRACE(
                     kTraceError,
                     kTraceVideo,
                     -1,
                     "DirectDraw HW does not have enough memory, freeMem:%d, requiredMem:%d",
                     ddcaps.dwVidMemFree, minVideoMemory);
        // If memory is not available on the Video Card...allocate it on RAM
    }
    else
    {
        WEBRTC_TRACE(kTraceInfo, kTraceVideo, -1,
                     "DirectDraw video memory, freeMem:%d, totalMem:%d",
                     ddcaps.dwVidMemFree, ddcaps.dwVidMemTotal);
    }

    /*
     DirectDrawCaps       ddsCaps ;
     ZeroMemory(&ddsCaps, sizeof(ddsCaps)) ;
     ddsCaps.dwCaps  = DDSCAPS_VIDEOMEMORY;
     DWORD memTotal=0;
     DWORD memFree=0;
     ddrval = _directDraw->GetAvailableVidMem(&ddsCaps, &memTotal, &memFree);
     if(ddrval == DD_OK)
     {
     WEBRTC_TRACE(kTraceInfo, kTraceVideo, -1, "DirectDraw video memory, freeMem:%d, totalMem:%d", memFree, memTotal);
     }
     */
    // Determine if the hardware supports overlay deinterlacing
    //	bCanDeinterlace = (ddcaps.dwCaps2 & DDCAPS2_CANFLIPODDEVEN) ? 1 : 0;

    // this fail since we check before we set the mode
    //	bool bCanFlip =(ddcaps.dwCaps & DDSCAPS_FLIP) ? 1 : 0;

    // Determine if the hardware supports colorkeying
    _supportTransparency = (ddcaps.dwCaps & DDCAPS_COLORKEY) ? 1 : 0;
    if (_supportTransparency)
    {
        WEBRTC_TRACE(kTraceInfo, kTraceVideo, -1,
                     "DirectDraw support colorkey");
    }
    else
    {
        WEBRTC_TRACE(kTraceWarning, kTraceVideo, -1,
                     "DirectDraw don't support colorkey");
    }

    if (ddcaps.dwCaps2 & DDCAPS2_CANRENDERWINDOWED)
    {
        //	required for _directDraw->FlipToGDISurface();
        WEBRTC_TRACE(kTraceInfo, kTraceVideo, -1,
                     "DirectDraw support CANRENDERWINDOWED");
    }
    else
    {
        WEBRTC_TRACE(kTraceInfo, kTraceVideo, -1,
                     "DirectDraw don't support CANRENDERWINDOWED");
    }

    // Determine if the hardware supports scaling during a blit
    _canStretch = (ddcaps.dwCaps & DDCAPS_BLTSTRETCH) ? 1 : 0;
    if (_canStretch)
    {
        WEBRTC_TRACE(kTraceInfo, kTraceVideo, -1,
                     "DirectDraw blit can stretch");
    }
    else
    {
        WEBRTC_TRACE(kTraceWarning, kTraceVideo, -1,
                     "DirectDraw blit can't stretch");
    }

    _canMirrorLeftRight = (ddcaps.dwFXAlphaCaps & DDBLTFX_MIRRORLEFTRIGHT) ? 1
            : 0;
    if (_canMirrorLeftRight)
    {
        WEBRTC_TRACE(kTraceInfo, kTraceVideo, -1,
                     "DirectDraw mirroring is supported");
    }
    else
    {
        WEBRTC_TRACE(kTraceInfo, kTraceVideo, -1,
                     "DirectDraw mirroring is not supported");
    }

    // Determine if the hardware supports color conversion during a blit
    _bCanBltFourcc = (ddcaps.dwCaps & DDCAPS_BLTFOURCC) ? 1 : 0;
    if (_bCanBltFourcc)
        _bCanBltFourcc = (ddcaps.dwCKeyCaps & DDCKEYCAPS_DESTBLT) ? 1 : 0;

    if (_bCanBltFourcc)
    {
        WEBRTC_TRACE(kTraceInfo, kTraceVideo, -1,
                     "DirectDraw can blit Fourcc");
        DWORD i_codes;
        ddrval = _directDraw->GetFourCCCodes(&i_codes, NULL);

        if (i_codes > 0)
        {
            DWORD* pi_codes = new DWORD[i_codes];

            ddrval = _directDraw->GetFourCCCodes(&i_codes, pi_codes);
            for (unsigned int i = 0; i < i_codes && _blitVideoType
                    != kI420; i++)
            {
                DWORD w = pi_codes[i];
                switch (w)
                {
                    case MAKEFOURCC('I', '4', '2', '0'):
                        //					_blitVideoType = kI420;
                        // not enabled since its not tested
                        WEBRTC_TRACE(kTraceInfo, kTraceVideo,
                                     -1, "DirectDraw support I420");
                        break;
                    case MAKEFOURCC('I', 'Y', 'U', 'V'): // same as YV12
                    //					_blitVideoType = kIYUV;
                        // not enabled since its not tested
                        WEBRTC_TRACE(kTraceInfo, kTraceVideo,
                                     -1, "DirectDraw support IYUV");
                        break;
                    case MAKEFOURCC('U', 'Y', 'N', 'V'): // same shit different name
                        WEBRTC_TRACE(kTraceInfo, kTraceVideo,
                                     -1, "DirectDraw support UYNV");
                        // not enabled since its not tested
                        break;
                    case MAKEFOURCC('Y', '4', '2', '2'): // same shit different name
                        WEBRTC_TRACE(kTraceInfo, kTraceVideo,
                                     -1, "DirectDraw support Y422");
                        // not enabled since its not tested
                        break;
                    case MAKEFOURCC('Y', 'U', 'N', 'V'): // same shit different name
                        WEBRTC_TRACE(kTraceInfo, kTraceVideo,
                                     -1, "DirectDraw support YUNV");
                        // not enabled since its not tested
                        break;
                    case MAKEFOURCC('Y', 'V', '1', '2'):
                        _blitVideoType = kYV12;
                        WEBRTC_TRACE(kTraceInfo, kTraceVideo,
                                     -1, "DirectDraw support YV12");
                        break;
                    case MAKEFOURCC('Y', 'U', 'Y', '2'):
                        if (_blitVideoType != kYV12)
                        {
                            _blitVideoType = kYUY2;
                        }
                        WEBRTC_TRACE(kTraceInfo, kTraceVideo,
                                     -1, "DirectDraw support YUY2");
                        break;
                    case MAKEFOURCC('U', 'Y', 'V', 'Y'):
                        if (_blitVideoType != kYV12)
                        {
                            _blitVideoType = kUYVY;
                        }
                        WEBRTC_TRACE(kTraceInfo, kTraceVideo,
                                     -1, "DirectDraw support UYVY");
                        break;
                    default:
                        WEBRTC_TRACE(kTraceInfo, kTraceVideo,
                                     -1, "DirectDraw unknown blit type %x", w);
                        break;
                }
            }
            delete[] pi_codes;
        }
    }
    return 0;
}

int VideoRenderDirectDraw::Stop()
{
    _confCritSect->Enter();

    _blit = false;

    _confCritSect->Leave();
    return 0;
}

bool VideoRenderDirectDraw::IsPrimaryOrMixingSurfaceOnSystem()
{
    return _isPrimaryOrMixingSurfaceOnSystem;
}

int VideoRenderDirectDraw::CreatePrimarySurface()
{
    // Create the primary surface
    DirectDrawSurfaceDesc ddsd;
    ZeroMemory(&ddsd, sizeof(ddsd));
    ddsd.dwSize = sizeof(ddsd);
    HRESULT ddrval = DD_OK;

    if (_directDraw == NULL)
    {
        WEBRTC_TRACE(kTraceError, kTraceVideo, -1,
                     "DirectDraw object not created");
        return -1;
        //return VIDEO_DIRECT_DRAW_FAILURE;
    }
    if (_primarySurface)
    {
        _primarySurface->Release();
        _primarySurface = NULL;
    }

    if (!_fullscreen)
    {
        // create a normal window
        ddrval = _directDraw->SetCooperativeLevel(_hWnd, DDSCL_NORMAL);
        if (FAILED(ddrval))
        {
            //******** Potential workaround for D#4608 *************** Ignore error.
            WEBRTC_TRACE(kTraceWarning, kTraceVideo, -1,
                         "DirectDraw failed to set SetCooperativeLevel %x, ddrval");
        }
        // we cant size the primary surface based on _hwndRect
        ddsd.dwFlags = DDSD_CAPS;
        ddsd.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE | DDSCAPS_VIDEOMEMORY;

#ifndef NOGRAPHICSCARD_MEMORY
        ddrval = _directDraw->CreateSurface(&ddsd, &_primarySurface, NULL);
        if (FAILED(ddrval))
        {
            WEBRTC_TRACE(
                         kTraceError,
                         kTraceVideo,
                         -1,
                         "DirectDraw failed to CreateSurface _primarySurface using VideoMemory: 0x%x",
                         ddrval);
            WEBRTC_TRACE(
                         kTraceError,
                         kTraceVideo,
                         -1,
                         "\t HWND: 0x%x, top: %d, left: %d, bottom: %d, right: %d, dwFlags: %d. Line : %d",
                         _hWnd, _hwndRect.top, _hwndRect.left,
                         _hwndRect.bottom, _hwndRect.right, ddsd.dwFlags,
                         __LINE__);

#endif
            //allocate using System memory
            ddsd.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE | DDSCAPS_SYSTEMMEMORY;
            ddrval = _directDraw->CreateSurface(&ddsd, &_primarySurface, NULL);
            if (FAILED(ddrval))
            {
                WEBRTC_TRACE(
                             kTraceError,
                             kTraceVideo,
                             -1,
                             "DirectDraw failed to CreateSurface _primarySurface using SystemMemory: 0x%x",
                             ddrval);
                if (ddrval != 0x887600E1)
                {
                    _directDraw->Release();
                    _directDraw = 0;
                }
                return -1;
                //return VIDEO_DIRECT_DRAW_FAILURE;
            }
            _isPrimaryOrMixingSurfaceOnSystem = true;
            WEBRTC_TRACE(kTraceInfo, kTraceVideo, -1,
                         "DirectDraw _primarySurface on SystemMemory");

#ifndef NOGRAPHICSCARD_MEMORY
        }
#endif

        // Create a clipper to ensure that our drawing stays inside our window
        LPDIRECTDRAWCLIPPER directDrawClipper;
        ddrval = _directDraw->CreateClipper(0, &directDrawClipper, NULL );
        if (ddrval != DD_OK)
        {
            WEBRTC_TRACE(kTraceError, kTraceVideo, -1,
                         "DirectDraw failed to CreateClipper");
            _primarySurface->Release();
            _directDraw->Release();
            _primarySurface = 0;
            _directDraw = 0;
            return -1;
            //return VIDEO_DIRECT_DRAW_FAILURE;
        }
        // setting it to our hwnd gives the clipper the coordinates from our window
        // when using cliplist we run into problem with transparent HWNDs (such as REX)
        ddrval = directDrawClipper->SetHWnd(0, _hWnd);
        if (ddrval != DD_OK)
        {
            WEBRTC_TRACE(kTraceError, kTraceVideo, -1,
                         "DirectDraw failed to SetHWnd");
            _primarySurface->Release();
            _directDraw->Release();
            _primarySurface = 0;
            _directDraw = 0;
            return -1;
            //return VIDEO_DIRECT_DRAW_FAILURE;
        }
        // attach the clipper to the primary surface
        ddrval = _primarySurface->SetClipper(directDrawClipper);
        directDrawClipper->Release(); // no need to keep the clipper around
        if (ddrval != DD_OK)
        {
            WEBRTC_TRACE(kTraceError, kTraceVideo, -1,
                         "DirectDraw failed to SetClipper");
            _primarySurface->Release();
            _directDraw->Release();
            _primarySurface = 0;
            _directDraw = 0;
            return -1;
            //return VIDEO_DIRECT_DRAW_FAILURE;
        }
    }
    else
    {
        /* The cooperative level determines how much control we have over the
         * screen. This must at least be either DDSCL_EXCLUSIVE or DDSCL_NORMAL
         *
         * DDSCL_EXCLUSIVE allows us to change video modes, and requires
         * the DDSCL_FULLSCREEN flag, which will cause the window to take over
         * the fullscreen. This is the preferred DirectDraw mode because it allows
         * us to have control of the whole screen without regard for GDI.
         *
         * DDSCL_NORMAL is used to allow the DirectDraw app to run windowed.
         */

        // Note: debuging in fullscreen mode does not work, thanks MS...
        ::GetWindowRect(_hWnd, &_originalHwndRect);

        // DDSCL_NOWINDOWCHANGES prevents DD to change the window but it give us trouble too, not using it
        ddrval = _directDraw->SetCooperativeLevel(_hWnd, DDSCL_EXCLUSIVE
                | DDSCL_FULLSCREEN | DDSCL_ALLOWREBOOT);

        if (FAILED(ddrval))
        {
            WEBRTC_TRACE(kTraceError, kTraceVideo, -1,
                         "DirectDraw failed to SetCooperativeLevel DDSCL_EXCLUSIVE");
            WEBRTC_TRACE(
                         kTraceError,
                         kTraceVideo,
                         -1,
                         "\t HWND: 0x%x, top: %d, left: %d, bottom: %d, right: %d, dwFlags: %d. Line : %d",
                         _hWnd, _hwndRect.top, _hwndRect.left,
                         _hwndRect.bottom, _hwndRect.right, ddsd.dwFlags,
                         __LINE__);

            _directDraw->Release();
            _directDraw = 0;
            return -1;
            //return VIDEO_DIRECT_DRAW_FAILURE;
        }
        ddsd.dwFlags = DDSD_CAPS | DDSD_BACKBUFFERCOUNT;
        ddsd.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE | DDSCAPS_FLIP
                | DDSCAPS_COMPLEX | DDSCAPS_VIDEOMEMORY;
        ddsd.dwBackBufferCount = 1;

        ddrval = _directDraw->CreateSurface(&ddsd, &_primarySurface, NULL);
        if (FAILED(ddrval))
        {
            WEBRTC_TRACE(
                         kTraceError,
                         kTraceVideo,
                         -1,
                         "DirectDraw failed to CreateSurface _primarySurface, fullscreen mode: 0x%x",
                         ddrval);
            WEBRTC_TRACE(
                         kTraceError,
                         kTraceVideo,
                         -1,
                         "\t HWND: 0x%x, top: %d, left: %d, bottom: %d, right: %d, dwFlags: %d. Line : %d",
                         _hWnd, _hwndRect.top, _hwndRect.left,
                         _hwndRect.bottom, _hwndRect.right, ddsd.dwFlags,
                         __LINE__);

            _directDraw->Release();
            _directDraw = 0;
            return -1;
            //return VIDEO_DIRECT_DRAW_FAILURE;
        }
        // Get a pointer to the back buffer
        DirectDrawCaps ddsCaps;
        ZeroMemory(&ddsCaps, sizeof(ddsCaps));
        ddsCaps.dwCaps = DDSCAPS_BACKBUFFER | DDSCAPS_VIDEOMEMORY;

        ddrval = _primarySurface->GetAttachedSurface(&ddsCaps, &_backSurface);
        if (FAILED(ddrval))
        {
            WEBRTC_TRACE(kTraceError, kTraceVideo, -1,
                         "DirectDraw failed to GetAttachedSurface, fullscreen mode ");
            WEBRTC_TRACE(
                         kTraceError,
                         kTraceVideo,
                         -1,
                         "\t HWND: 0x%x, top: %d, left: %d, bottom: %d, right: %d, dwFlags: %d. Line : %d",
                         _hWnd, _hwndRect.top, _hwndRect.left,
                         _hwndRect.bottom, _hwndRect.right, ddsd.dwFlags,
                         __LINE__);

            _primarySurface->Release();
            _directDraw->Release();
            _primarySurface = 0;
            _directDraw = 0;
            return -1;
            //return VIDEO_DIRECT_DRAW_FAILURE;
        }
        // Get the screen size and save it as a rect
        ZeroMemory(&ddsd, sizeof(ddsd));
        ddsd.dwSize = sizeof(ddsd);
    }

    ZeroMemory(&ddsd, sizeof(ddsd));
    ddsd.dwSize = sizeof(ddsd);

    // get our prinmary surface description
    ddrval = _primarySurface->GetSurfaceDesc(&ddsd);
    if (!(SUCCEEDED(ddrval) && (ddsd.dwFlags & DDSD_WIDTH) && (ddsd.dwFlags
            & DDSD_HEIGHT)))
    {
        WEBRTC_TRACE(kTraceError, kTraceVideo, -1,
                     "DirectDraw failed to GetSurfaceDesc _primarySurface");
        WEBRTC_TRACE(
                     kTraceError,
                     kTraceVideo,
                     -1,
                     "\t HWND: 0x%x, top: %d, left: %d, bottom: %d, right: %d, dwFlags: %d. Line : %d",
                     _hWnd, _hwndRect.top, _hwndRect.left, _hwndRect.bottom,
                     _hwndRect.right, ddsd.dwFlags, __LINE__);

        _primarySurface->Release();
        _directDraw->Release();
        _primarySurface = 0;
        _directDraw = 0;
        return -1;
        //return VIDEO_DIRECT_DRAW_FAILURE;
    }
    // first we need to figure out the size of the primary surface

    // store screen size
    ::SetRect(&_screenRect, 0, 0, ddsd.dwWidth, ddsd.dwHeight);

    // store RGB type
    if (ddsd.ddpfPixelFormat.dwFlags & DDPF_RGB)
    {
        // RGB surface
        switch (ddsd.ddpfPixelFormat.dwRGBBitCount)
        {
            case 16:
                switch (ddsd.ddpfPixelFormat.dwGBitMask)
                {
                    case 0x00e0:
                        _rgbVideoType = kARGB4444;
                        break;
                    case 0x03e0:
                        _rgbVideoType = kARGB1555;
                        break;
                    case 0x07e0:
                        _rgbVideoType = kRGB565;
                        break;
                }
                break;
            case 24:
                _rgbVideoType = kRGB24;
                break;
            case 32:
                _rgbVideoType = kARGB;
                break;
        }
    }
    switch (_blitVideoType)
    {
        case kI420:
        case kIYUV:
        case kYUY2:
        case kYV12:
        case kUYVY:
            _incomingVideoType = kI420;
            break;
        case kUnknown:
            _blitVideoType = _rgbVideoType;
            _incomingVideoType = kI420;
            break;
        default:
            _blitVideoType = _rgbVideoType;
            _incomingVideoType = kI420;
            break;
    }
    WEBRTC_TRACE(
                 kTraceInfo,
                 kTraceVideo,
                 -1,
                 "DirectDraw created _primarySurface, _blitVideoType %d, _rgbvideoType %d",
                 _blitVideoType, _rgbVideoType);
    return 0;
}

int VideoRenderDirectDraw::CreateMixingSurface()
{
    if (_directDraw == NULL)
    {
        WEBRTC_TRACE(kTraceError, kTraceVideo, -1,
                     "DirectDraw object not created");
        return -1;
        //return VIDEO_DIRECT_DRAW_FAILURE;
    }

    if (_fullscreen)
    {
        ::CopyRect(&_hwndRect, &_screenRect);
    }
    else
    {
        // update our _hWnd size
        ::GetClientRect(_hWnd, &_hwndRect);
    }

    if (_mixingSurface)
    {
        _mixingSurface->Release();
        _mixingSurface = NULL;
    }
    // create mixing surface
    DirectDrawSurfaceDesc ddsd;
    memset(&ddsd, 0, sizeof(ddsd));
    ddsd.dwSize = sizeof(ddsd);
    ddsd.dwFlags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH;
    ddsd.ddsCaps.dwCaps = DDSCAPS_VIDEOMEMORY;
    ddsd.dwHeight = _hwndRect.bottom;
    ddsd.dwWidth = _hwndRect.right;

    /*    char logStr[256];
     _snprintf(logStr,256, "CreateMixingSurface H:%d W:%d \n",_hwndRect.bottom, _hwndRect.right);
     OutputDebugString(logStr);
     */

#ifndef NOGRAPHICSCARD_MEMORY
    HRESULT ddrval = _directDraw->CreateSurface(&ddsd, &_mixingSurface, NULL);
    if (FAILED(ddrval))
    {
        WEBRTC_TRACE(
                     kTraceError,
                     kTraceVideo,
                     -1,
                     "DirectDraw failed to CreateSurface _mixingSurface using VideoMemory: 0x%x",
                     ddrval);
        WEBRTC_TRACE(
                     kTraceError,
                     kTraceVideo,
                     -1,
                     "\t HWND: 0x%x, top: %d, left: %d, bottom: %d, right: %d, dwFlags: %d",
                     _hWnd, _hwndRect.top, _hwndRect.left, _hwndRect.bottom,
                     _hwndRect.right, ddsd.dwFlags);
#endif

        ddsd.ddsCaps.dwCaps = DDSCAPS_SYSTEMMEMORY;
        HRESULT ddrval = _directDraw->CreateSurface(&ddsd, &_mixingSurface,
                                                    NULL);
        if (FAILED(ddrval))
        {
            WEBRTC_TRACE(
                         kTraceError,
                         kTraceVideo,
                         -1,
                         "DirectDraw failed to CreateSurface _mixingSurface on System Memory: 0x%x",
                         ddrval);
            return -1;
            //return VIDEO_DIRECT_DRAW_FAILURE;
        }
        _isPrimaryOrMixingSurfaceOnSystem = true;
        WEBRTC_TRACE(kTraceError, kTraceVideo, -1,
                     "DirectDraw CreateSurface _mixingSurface on SystemMemory");

#ifndef NOGRAPHICSCARD_MEMORY        
    }
#endif

    _clearMixingSurface = true;
    WEBRTC_TRACE(kTraceInfo, kTraceVideo, -1,
                 "DirectDraw _mixingSurface created");
    return 0;
}

VideoRenderCallback* VideoRenderDirectDraw::CreateChannel(WebRtc_UWord32 channel,
                                                                  WebRtc_UWord32 zOrder,
                                                                  float startWidth,
                                                                  float startHeight,
                                                                  float stopWidth,
                                                                  float stopHeight)
{
    if (!_canStretch)
    {
        if (startWidth != 0.0f || startHeight != 0.0f || stopWidth != 1.0f
                || stopHeight != 1.0f)
        {
            WEBRTC_TRACE(kTraceError, kTraceVideo, -1,
                         "DirectDraw failed to CreateChannel HW don't support stretch");
            return NULL;
        }
    }
    DirectDrawChannel* ddobj =
            new DirectDrawChannel(_directDraw, _blitVideoType,
                                      _incomingVideoType, _rgbVideoType, this);
    ddobj->SetStreamSettings(this, 0, startWidth, startHeight, stopWidth,
                             stopHeight);

    // store channel
    _directDrawChannels[channel & 0x0000ffff] = ddobj;

    // store Z order
    // default streamID is 0
    _directDrawZorder.insert(ZorderPair(zOrder, channel & 0x0000ffff));
    return ddobj;
}

int VideoRenderDirectDraw::AddDirectDrawChannel(int channel,
                                                    unsigned char streamID,
                                                    int zOrder,
                                                    DirectDrawChannel* ddObj)
{
    // Only allow one stream per channel, demuxing is done outside of DirectDraw...
    streamID = 0;
    unsigned int streamChannel = (streamID << 16) + (channel & 0x0000ffff);

    // store channel
    _directDrawChannels[channel & 0x0000ffff] = ddObj;

    _demuxing = true; // with this function it's always demux

    // store Z order
    _directDrawZorder.insert(ZorderPair(zOrder, streamChannel));
    return 0;
}

DirectDrawChannel* VideoRenderDirectDraw::ShareDirectDrawChannel(
                                                                         int channel)
{
    CriticalSectionScoped cs(_confCritSect);

    DirectDrawChannel* obj = NULL;

    std::map<int, DirectDrawChannel*>::iterator ddIt;
    ddIt = _directDrawChannels.find(channel & 0x0000ffff);
    if (ddIt != _directDrawChannels.end())
    {
        obj = ddIt->second;
        obj->AddRef();
    }
    return obj;
}

WebRtc_Word32 VideoRenderDirectDraw::DeleteChannel(const WebRtc_UWord32 channel)
{
    CriticalSectionScoped cs(_confCritSect);

    // Remove the old z order

    //unsigned int streamChannel = (streamID << 16) + (channel & 0x0000ffff);	
    std::multimap<int, unsigned int>::iterator it;
    it = _directDrawZorder.begin();
    while (it != _directDrawZorder.end())
    {
        //if(streamChannel == it->second )
        if ((channel & 0x0000ffff) == (it->second & 0x0000ffff))
        {
            it = _directDrawZorder.erase(it);
            break;
        }
        it++;
    }

    std::map<int, DirectDrawChannel*>::iterator ddIt;
    ddIt = _directDrawChannels.find(channel & 0x0000ffff);
    if (ddIt != _directDrawChannels.end())
    {
        ddIt->second->Release();
        _directDrawChannels.erase(ddIt);
        _clearMixingSurface = true;
    }

    return 0;
}

WebRtc_Word32 VideoRenderDirectDraw::GetStreamSettings(const WebRtc_UWord32 channel,
                                                           const WebRtc_UWord16 streamId,
                                                           WebRtc_UWord32& zOrder,
                                                           float& startWidth,
                                                           float& startHeight,
                                                           float& stopWidth,
                                                           float& stopHeight)
{
    CriticalSectionScoped cs(_confCritSect);

    std::map<int, DirectDrawChannel*>::iterator ddIt;
    ddIt = _directDrawChannels.find(channel & 0x0000ffff);
    if (ddIt == _directDrawChannels.end())
    {
        // This channel doesn't exist.
        return -1;
    }

    DirectDrawChannel* ptrChannel = ddIt->second;
    // Only support one stream per channel, is demuxing done outside if DD.
    //if (ptrChannel->GetStreamSettings(this, streamId, startWidth, startHeight, stopWidth, stopHeight) == -1)
    if (ptrChannel->GetStreamSettings(this, 0, startWidth, startHeight,
                                      stopWidth, stopHeight) == -1)
    {
        // Error for this stream
        return -1;
    }

    // Get the zOrder
    std::multimap<int, unsigned int>::iterator it;
    it = _directDrawZorder.begin();
    while (it != _directDrawZorder.end())
    {
        if ((channel & 0x0000ffff) == (it->second & 0x0000ffff))
        {
            // We found our channel zOrder
            zOrder = (unsigned int) (it->first);
            break;
        }
        it++;
    }

    return 0;
}

int VideoRenderDirectDraw::GetChannels(std::list<int>& channelList)
{
    CriticalSectionScoped cs(_confCritSect);

    std::map<int, DirectDrawChannel*>::iterator ddIt;
    ddIt = _directDrawChannels.begin();

    while (ddIt != _directDrawChannels.end())
    {
        int channel = ddIt->first;
        if (channel == 0x0000ffff)
        {
            channel = -1;
        }
        channelList.push_back(channel);
        ddIt++;
    }
    return 0;
}

bool VideoRenderDirectDraw::HasChannel(int channel)
{
    CriticalSectionScoped cs(_confCritSect);

    std::map<int, DirectDrawChannel*>::iterator ddIt;
    ddIt = _directDrawChannels.find(channel & 0x0000ffff);
    if (ddIt != _directDrawChannels.end())
    {
        return true;
    }
    return false;
}

bool VideoRenderDirectDraw::HasChannels()
{
    CriticalSectionScoped cs(_confCritSect);

    if (_directDrawChannels.begin() != _directDrawChannels.end())
    {
        return true;
    }
    return false;
}

bool VideoRenderDirectDraw::IsFullScreen()
{
    return _fullscreen;
}

VideoType VideoRenderDirectDraw::GetPerferedVideoFormat()
{
    return _incomingVideoType;
}

// this can be called rutime from another thread
DirectDrawChannel* VideoRenderDirectDraw::ConfigureDirectDrawChannel(int channel,
                                                                             unsigned char streamID,
                                                                             int zOrder,
                                                                             float left,
                                                                             float top,
                                                                             float right,
                                                                             float bottom)
{
    // Only support one stream per channel, is demuxing done outside if DD.
    streamID = 0;

    CriticalSectionScoped cs(_confCritSect);

    if (!_canStretch)
    {
        if (left != 0.0f || top != 0.0f || right != 1.0f || bottom != 1.0f)
        {
            WEBRTC_TRACE(kTraceError, kTraceVideo, -1,
                         "DirectDraw failed to ConfigureDirectDrawChannel HW don't support stretch");
            return NULL;
        }
    }
    std::map<int, DirectDrawChannel*>::iterator ddIt;
    ddIt = _directDrawChannels.find(channel & 0x0000ffff);
    DirectDrawChannel* ddobj = NULL;
    if (ddIt != _directDrawChannels.end())
    {
        ddobj = ddIt->second;
    }
    if (ddobj == NULL)
    {
        WEBRTC_TRACE(kTraceError, kTraceVideoRenderer, -1,
                     "DirectDraw failed to find channel");
        return NULL;
    }
    unsigned int streamChannel = (streamID << 16) + (channel & 0x0000ffff);
    // remove the old z order
    std::multimap<int, unsigned int>::iterator it;
    it = _directDrawZorder.begin();
    while (it != _directDrawZorder.end())
    {
        if (streamChannel == it->second)
        {
            it = _directDrawZorder.erase(it);
            break;
        }
        it++;
    }
    // if this channel already are in the zOrder map it's demux
    it = _directDrawZorder.begin();
    while (it != _directDrawZorder.end())
    {
        if (channel == (it->second & 0x0000ffff))
        {
            _demuxing = true;
            break;
        }
        it++;
    }
    if (it == _directDrawZorder.end())
    {
        _demuxing = false;
    }

    _clearMixingSurface = true;

    if (left == 0.0f && top == 0.0f && right == 0.0f && bottom == 0.0f)
    {
        // remove
        _directDrawChannels.erase(ddIt);
        ddobj->Release();
        return NULL;
    }
    ddobj->SetStreamSettings(this, streamID, left, top, right, bottom);

    _directDrawZorder.insert(ZorderPair(zOrder, streamChannel));
    return ddobj;
}

WebRtc_Word32 VideoRenderDirectDraw::SetCropping(const WebRtc_UWord32 channel,
                                                     const WebRtc_UWord16 streamID,
                                                     float left, float top,
                                                     float right, float bottom)
{
    CriticalSectionScoped cs(_confCritSect);
    if (!_canStretch)
    {
        if (left != 0.0f || top != 0.0f || right != 1.0f || bottom != 1.0f)
        {
            WEBRTC_TRACE(kTraceError, kTraceVideoRenderer, -1,
                         "DirectDraw failed to SetCropping HW don't support stretch");
            return -1;
            //return VIDEO_DIRECT_DRAW_FAILURE;
        }
    }

    std::map<int, DirectDrawChannel*>::iterator ddIt;
    ddIt = _directDrawChannels.find(channel & 0x0000ffff);
    if (ddIt != _directDrawChannels.end())
    {
        DirectDrawChannel* ddobj = ddIt->second;
        if (ddobj)
        {
            // Only support one stream per channel, is demuxing done outside if DD.
            ddobj->SetStreamCropSettings(this, 0, left, top, right, bottom);
            //ddobj->SetStreamCropSettings(this, streamID, left, top, right, bottom);
        }
    }
    return 0;
}

WebRtc_Word32 VideoRenderDirectDraw::ConfigureRenderer(const WebRtc_UWord32 channel,
                                                           const WebRtc_UWord16 streamId,
                                                           const unsigned int zOrder,
                                                           const float left,
                                                           const float top,
                                                           const float right,
                                                           const float bottom)
{
    if (ConfigureDirectDrawChannel(channel, (unsigned char) streamId, zOrder,
                                   left, top, right, bottom) == NULL)
    {
        if (left == 0.0f && top == 0.0f && right == 0.0f && bottom == 0.0f)
        {
            WEBRTC_TRACE(kTraceInfo, kTraceVideoRenderer, -1,
                         "ConfigureRender, removed channel:%d streamId:%d",
                         channel, streamId);
        }
        else
        {
            WEBRTC_TRACE(
                         kTraceError,
                         kTraceVideoRenderer,
                         -1,
                         "DirectDraw failed to ConfigureRenderer for channel: %d",
                         channel);
            return -1;
        }
    }
    return 0;
}

// this can be called runtime from another thread
WebRtc_Word32 VideoRenderDirectDraw::SetText(const WebRtc_UWord8 textId,
                                                 const WebRtc_UWord8* text,
                                                 const WebRtc_Word32 textLength,
                                                 const WebRtc_UWord32 colorText,
                                                 const WebRtc_UWord32 colorBg,
                                                 const float left,
                                                 const float top,
                                                 const float right,
                                                 const float bottom)
{
    DirectDrawTextSettings* textSetting = NULL;

    CriticalSectionScoped cs(_confCritSect);

    _frameChanged = true;

    std::map<unsigned char, DirectDrawTextSettings*>::iterator it;
    it = _textSettings.find(textId);
    if (it != _textSettings.end())
    {
        if (it->second)
        {
            textSetting = it->second;
        }
    }
    _clearMixingSurface = true;

    if (text == NULL || textLength == 0)
    {
        WEBRTC_TRACE(kTraceInfo, kTraceVideo, -1,
                     "DirectDraw remove text textId:%d", textId);
        if (textSetting)
        {
            delete textSetting;
            _textSettings.erase(it);
        }
        return 0;
    }

    // sanity
    if (left > 1.0f || left < 0.0f)
    {
        WEBRTC_TRACE(kTraceError, kTraceVideo, -1,
                     "DirectDraw SetText invalid parameter");
        return -1;
        //return VIDEO_DIRECT_DRAW_INVALID_ARG;
    }
    if (top > 1.0f || top < 0.0f)
    {
        WEBRTC_TRACE(kTraceError, kTraceVideo, -1,
                     "DirectDraw SetText invalid parameter");
        return -1;
        //return VIDEO_DIRECT_DRAW_INVALID_ARG;
    }
    if (right > 1.0f || right < 0.0f)
    {
        WEBRTC_TRACE(kTraceError, kTraceVideo, -1,
                     "DirectDraw SetText invalid parameter");
        return -1;
        //return VIDEO_DIRECT_DRAW_INVALID_ARG;
    }
    if (bottom > 1.0f || bottom < 0.0f)
    {
        WEBRTC_TRACE(kTraceError, kTraceVideo, -1,
                     "DirectDraw SetText invalid parameter");
        return -1;
        //return VIDEO_DIRECT_DRAW_INVALID_ARG;
    }
    if (textSetting == NULL)
    {
        textSetting = new DirectDrawTextSettings();
    }
    int retVal = textSetting->SetText((const char*) text, textLength,
                                      (COLORREF) colorText, (COLORREF) colorBg,
                                      left, top, right, bottom);
    if (retVal != 0)
    {
        delete textSetting;
        textSetting = NULL;
        _textSettings.erase(textId);
        return retVal;
    }
    if (textSetting)
    {
        _textSettings[textId] = textSetting;
    }
    return retVal;
}

// this can be called runtime from another thread
WebRtc_Word32 VideoRenderDirectDraw::SetBitmap(const void* bitMap,
                                                   const WebRtc_UWord8 pictureId,
                                                   const void* colorKey,
                                                   const float left,
                                                   const float top,
                                                   const float right,
                                                   const float bottom)
{
    DirectDrawBitmapSettings* bitmapSetting = NULL;

    CriticalSectionScoped cs(_confCritSect);

    _frameChanged = true;
    std::map<unsigned char, DirectDrawBitmapSettings*>::iterator it;
    it = _bitmapSettings.find(pictureId);
    if (it != _bitmapSettings.end())
    {
        if (it->second)
        {
            bitmapSetting = it->second;
        }
    }
    _clearMixingSurface = true;

    if (bitMap == NULL)
    {
        WEBRTC_TRACE(kTraceInfo, kTraceVideo, -1,
                     "DirectDraw remove bitmap pictureId:%d", pictureId);
        if (bitmapSetting)
        {
            delete bitmapSetting;
            _bitmapSettings.erase(it);
        }
        return 0;
    }

    // sanity
    if (left > 1.0f || left < 0.0f)
    {
        WEBRTC_TRACE(kTraceError, kTraceVideo, -1,
                     "DirectDraw SetBitmap invalid parameter");
        return -1;
        //return VIDEO_DIRECT_DRAW_INVALID_ARG;
    }
    if (top > 1.0f || top < 0.0f)
    {
        WEBRTC_TRACE(kTraceError, kTraceVideo, -1,
                     "DirectDraw SetBitmap invalid parameter");
        return -1;
        //return VIDEO_DIRECT_DRAW_INVALID_ARG;
    }
    if (right > 1.0f || right < 0.0f)
    {
        WEBRTC_TRACE(kTraceError, kTraceVideo, -1,
                     "DirectDraw SetBitmap invalid parameter");
        return -1;
        //return VIDEO_DIRECT_DRAW_INVALID_ARG;
    }
    if (bottom > 1.0f || bottom < 0.0f)
    {
        WEBRTC_TRACE(kTraceError, kTraceVideo, -1,
                     "DirectDraw SetBitmap invalid parameter");
        return -1;
        //return VIDEO_DIRECT_DRAW_INVALID_ARG;
    }
    if (!_canStretch)
    {
        if (left != 0.0f || top != 0.0f || right != 1.0f || bottom != 1.0f)
        {
            WEBRTC_TRACE(kTraceError, kTraceVideo, -1,
                         "DirectDraw failed to SetBitmap HW don't support stretch");
            return -1;
            //return VIDEO_DIRECT_DRAW_INVALID_ARG;
        }
    }
    if (bitmapSetting == NULL)
    {
        bitmapSetting = new DirectDrawBitmapSettings();
    }

    bitmapSetting->_transparentBitMap = (HBITMAP) bitMap;
    bitmapSetting->_transparentBitmapLeft = left;
    bitmapSetting->_transparentBitmapRight = right;
    bitmapSetting->_transparentBitmapTop = top;
    bitmapSetting->_transparentBitmapBottom = bottom;

    // colorKey == NULL equals no transparency
    if (colorKey)
    {
        // first remove constness
        DDCOLORKEY* ddColorKey =
                static_cast<DDCOLORKEY*> (const_cast<void*> (colorKey));
        if (!_supportTransparency)
        {
            WEBRTC_TRACE(kTraceError, kTraceVideo, -1,
                         "DirectDraw failed to SetBitmap HW don't support transparency");
            return -1;
            //return VIDEO_DIRECT_DRAW_INVALID_ARG;
        }
        if (bitmapSetting->_transparentBitmapColorKey == NULL)
        {
            bitmapSetting->_transparentBitmapColorKey = new DDCOLORKEY();
        }

        if (ddColorKey)
        {
            bitmapSetting->_transparentBitmapColorKey->dwColorSpaceLowValue
                    = ddColorKey->dwColorSpaceLowValue;
            bitmapSetting->_transparentBitmapColorKey->dwColorSpaceHighValue
                    = ddColorKey->dwColorSpaceHighValue;
        }
    }
    int retval = bitmapSetting->SetBitmap(_trace, _directDraw);
    if (retval != 0)
    {
        delete bitmapSetting;
        bitmapSetting = NULL;
        _bitmapSettings.erase(pictureId);
        return retval;
    }
    if (bitmapSetting)
    {
        _bitmapSettings[pictureId] = bitmapSetting;
    }
    return retval;
}

// this can be called rutime from another thread
WebRtc_Word32 VideoRenderDirectDraw::SetTransparentBackground(
                                                                  const bool enable)
{
    CriticalSectionScoped cs(_confCritSect);

    if (_supportTransparency)
    {
        _transparentBackground = enable;
        if (enable)
        {
            WEBRTC_TRACE(kTraceInfo, kTraceVideo, -1,
                         "DirectDraw enabled TransparentBackground");
        }
        else
        {
            WEBRTC_TRACE(kTraceInfo, kTraceVideo, -1,
                         "DirectDraw disabled TransparentBackground");
        }
        return 0;
    }
    WEBRTC_TRACE(
                 kTraceError,
                 kTraceVideo,
                 -1,
                 "DirectDraw failed to EnableTransparentBackground HW don't support transparency");
    return -1;
    //return VIDEO_DIRECT_DRAW_INVALID_ARG;
}

int VideoRenderDirectDraw::FillSurface(DirectDrawSurface *pDDSurface,
                                           RECT* rect)
{
    // sanity checks
    if (NULL == pDDSurface)
    {
        return -1;
        //return VIDEO_DIRECT_DRAW_FAILURE;
    }
    if (NULL == rect)
    {
        return -1;
        //return VIDEO_DIRECT_DRAW_FAILURE;
    }

    // Repaint the whole specified surface
    HRESULT ddrval;
    DDBLTFX ddFX;

    ZeroMemory(&ddFX, sizeof(ddFX));
    ddFX.dwSize = sizeof(ddFX);
    ddFX.dwFillColor = RGB(0, 0, 0);

    // Draw color key on the video area of given surface
    ddrval = pDDSurface->Blt(rect, NULL, NULL, DDBLT_COLORFILL | DDBLT_WAIT,
                             &ddFX);
    if (FAILED(ddrval))
    {
        WEBRTC_TRACE(kTraceError, kTraceVideo, -1,
                     "DirectDraw failed to fill surface");
        return -1;
        //return VIDEO_DIRECT_DRAW_FAILURE;
    }
    return 0;
}

// the real rendering thread
bool VideoRenderDirectDraw::RemoteRenderingThreadProc(void *obj)
{
    return static_cast<VideoRenderDirectDraw*> (obj)->RemoteRenderingProcess();
}

bool VideoRenderDirectDraw::RemoteRenderingProcess()
{
    bool hwndChanged = false;
    int waitTime = 0;

    _screenEvent->Wait(100);

    _confCritSect->Enter();

    if (_blit == false)
    {
        _confCritSect->Leave();
        return true;
    }

    if (!::GetForegroundWindow())
    {
        //no window, i.e the user have clicked CTRL+ALT+DEL, return true and wait
        _confCritSect->Leave();
        return true;
    }

    // Skip to blit if last render to primare surface took too long time.
    _processCount++;
    if (_deliverInQuarterFrameRate)
    {
        if (_processCount % 4 != 0)
        {
            _confCritSect->Leave();
            return true;
        }
    }
    else if (_deliverInHalfFrameRate)
    {
        if (_processCount % 2 != 0)
        {
            _confCritSect->Leave();
            return true;
        }
    }

    // Calculate th erender process time
    unsigned int startProcessTime = timeGetTime();

    hwndChanged = HasHWNDChanged();
    if (hwndChanged)
    {
        _clearMixingSurface = true;
    }

    std::map<int, DirectDrawChannel*>::iterator it;
    it = _directDrawChannels.begin();
    while (it != _directDrawChannels.end() && !_frameChanged)
    {
        if (it->second)
        {
            _frameChanged = it->second->IsOffScreenSurfaceUpdated(this);
        }
        it++;
    }
    if (_backSurface)
    {
        if (hwndChanged || _frameChanged)
        {
            BlitFromOffscreenBuffersToMixingBuffer();
            BlitFromBitmapBuffersToMixingBuffer();
            BlitFromTextToMixingBuffer();
        }
        BlitFromMixingBufferToBackBuffer();
        WaitAndFlip(waitTime);
    }
    else
    {
        if (hwndChanged || _frameChanged)
        {
            BlitFromOffscreenBuffersToMixingBuffer();
            BlitFromBitmapBuffersToMixingBuffer();
            BlitFromTextToMixingBuffer();
        }
        BlitFromMixingBufferToFrontBuffer(hwndChanged, waitTime);

    }
    // Check the total time it took processing all rendering. Don't consider waitTime.
    //const int totalRenderTime=GET_TIME_IN_MS()- startProcessTime-waitTime;            
    const int totalRenderTime = ::timeGetTime() - startProcessTime - waitTime;
    DecideBestRenderingMode(hwndChanged, totalRenderTime);
    _frameChanged = false;
    _confCritSect->Leave();

    return true;
}
void VideoRenderDirectDraw::DecideBestRenderingMode(bool hwndChanged,
                                                        int totalRenderTime)
{
    /* Apply variuos fixes for bad graphic drivers.
     1. If cpu to high- test wait fix
     2. If cpu still too high render in 1/2 display update period.
     3. If RemoteRenderingProcess take to long time reduce the blit period to 1/2 display update period.
     4. If RemoteRenderingProcess still take to long time try color conversion fix. It do color conversion in VieoRenderDirectDrawChannel::DeliverFrame
     5. If RemoteRenderingProcess still take to long time reduce the blit period to 1/4 display update period and disable color conversion fix.
     6  if  RemoteRenderingProcess still take to long time reduce the blit period to 1/4 display update period and enable color conversion fix again.
     */

    const int timesSinceLastCPUCheck = timeGetTime()
            - _screenRenderCpuUsage.LastGetCpuTime();
    int cpu = 0;

    if (hwndChanged) // Render window changed.
    {
        cpu = _screenRenderCpuUsage.GetCpuUsage(); // Get CPU usage for this thread. (Called if hwndCanged just to reset the GET CPU Usage function)
        _nrOfTooLongRenderTimes = 0; // Reset count of too long render times.
        return; // Return - nothing more to do since the window has changed.
    }
    // Check total rendering times
    if (_maxAllowedRenderTime > 0 && totalRenderTime > _maxAllowedRenderTime)
    {
        if (!_deliverInHalfFrameRate || totalRenderTime > 2
                * _maxAllowedRenderTime)
        {
            _nrOfTooLongRenderTimes += totalRenderTime / _maxAllowedRenderTime; //Weighted with the number of to long render times
        }
    }

    // If we are not using back surface (ie full screen rendering) we might try to switch BlitFromMixingBufferToFrontBuffer mode. 
    if (timesSinceLastCPUCheck > WindowsThreadCpuUsage::CPU_CHECK_INTERVAL)
    {
        cpu = _screenRenderCpuUsage.GetCpuUsage(); // Get CPU usage for this thread. (Called if hwndCanged just to reset the GET CPU Usage function)
        WEBRTC_TRACE(
                     kTraceStream,
                     kTraceVideo,
                     -1,
                     "Screen render thread cpu usage. (Tid %d), cpu usage %d processTime %d, no of too long render times %d",
                     GetCurrentThreadId(), cpu, totalRenderTime,
                     _nrOfTooLongRenderTimes);

        // If this screen render thread uses more than 5% of the total CPU time and the 
        // 1. try waitFix     
        if (cpu >= 5 && _renderModeWaitForCorrectScanLine == false
                && !_backSurface)
        {
            WEBRTC_TRACE(
                         kTraceWarning,
                         kTraceVideo,
                         -1,
                         "HIGH screen render thread cpu usage. (Tid %d), cpu usage %d, applying wait for scan line",
                         GetCurrentThreadId(), cpu);
            _renderModeWaitForCorrectScanLine = true;
            _fullScreenWaitEvent->StartTimer(true, 1);
        }
        else if (cpu >= 10 && _deliverInHalfFrameRate == false)
        {
            WEBRTC_TRACE(
                         kTraceWarning,
                         kTraceVideo,
                         -1,
                         "HIGH screen render thread cpu usage. (Tid %d), cpu usage %d, Render half rate",
                         GetCurrentThreadId(), cpu);
            _deliverInHalfFrameRate = true;
        }
        else
        {
            // Check if rendering takes too long time
            if (_nrOfTooLongRenderTimes > 15 || totalRenderTime
                    >= WindowsThreadCpuUsage::CPU_CHECK_INTERVAL)
            {

                // The rendering is taking too long time
                if (_deliverInHalfFrameRate == false)
                {
                    WEBRTC_TRACE(kTraceInfo, kTraceVideo, -1,
                                 "Render half rate, tid: %d",
                                 GetCurrentThreadId());
                    _deliverInHalfFrameRate = true;
                }
                else if (_deliverInScreenType == false
                        && !_deliverInQuarterFrameRate)
                {
                    WEBRTC_TRACE(
                                 kTraceInfo,
                                 kTraceVideo,
                                 -1,
                                 "Applying deliver in screen type format, tid: %d",
                                 GetCurrentThreadId());
                    // 2. try RGB fix
                    std::map<int, DirectDrawChannel*>::iterator it;
                    it = _directDrawChannels.begin();
                    while (it != _directDrawChannels.end())
                    {
                        it->second->ChangeDeliverColorFormat(true);
                        it++;
                    }
                    _deliverInScreenType = true;
                }
                else if (_deliverInQuarterFrameRate == false)
                {
                    WEBRTC_TRACE(
                                 kTraceInfo,
                                 kTraceVideo,
                                 -1,
                                 "Render quarter rate and disable deliver in screen type format, tid: %d",
                                 GetCurrentThreadId());
                    _deliverInQuarterFrameRate = true;
                    if (_deliverInScreenType)
                    {
                        //Disable  RGB fix
                        std::map<int, DirectDrawChannel*>::iterator it;
                        it = _directDrawChannels.begin();
                        while (it != _directDrawChannels.end())
                        {
                            it->second->ChangeDeliverColorFormat(false);
                            it++;
                        }
                        _deliverInScreenType = false;
                    }
                }
                else if (_deliverInQuarterFrameRate == true
                        && !_deliverInScreenType)
                {
                    WEBRTC_TRACE(
                                 kTraceInfo,
                                 kTraceVideo,
                                 -1,
                                 "Render quarter rate and enable RGB fix, tid: %d",
                                 GetCurrentThreadId());
                    _deliverInQuarterFrameRate = true;

                    //Enabe  RGB fix
                    std::map<int, DirectDrawChannel*>::iterator it;
                    it = _directDrawChannels.begin();
                    while (it != _directDrawChannels.end())
                    {
                        it->second->ChangeDeliverColorFormat(true);
                        it++;
                    }
                    _deliverInScreenType = true;
                }
            }
        }
        _nrOfTooLongRenderTimes = 0; // Reset count of too long render times.
    }
}

/*
 *	Internal help functions for blitting
 */

bool VideoRenderDirectDraw::HasHWNDChanged()
{
    //	we check if the HWND has changed 
    if (!_fullscreen)
    {
        RECT currentRect;
        ::GetClientRect(_hWnd, &currentRect);
        if (!EqualRect(&currentRect, &_hwndRect))
        {
            int retVal = CreateMixingSurface(); // this will delete the old mixing surface
            if (retVal != 0)
            {
                return false;
            }
            return true;
        }
    }
    return false;
}

int VideoRenderDirectDraw::BlitFromOffscreenBuffersToMixingBuffer()
{
    bool updateAll = false; // used to minimize the number of blt

    DDBLTFX ddbltfx;
    ZeroMemory(&ddbltfx, sizeof(ddbltfx));
    ddbltfx.dwSize = sizeof(ddbltfx);
    ddbltfx.dwDDFX = DDBLTFX_NOTEARING;

    if (_mixingSurface == NULL)
    {
        int retVal = CreateMixingSurface();
        if (retVal != 0)
        {
            // trace done
            return retVal;
        }
    }
    RECT mixingRect;
    ::SetRectEmpty(&mixingRect);

    if (_fullscreen)
    {
        ::CopyRect(&mixingRect, &_screenRect);
    }
    else
    {
        ::CopyRect(&mixingRect, &_hwndRect);
        // what if largest size is larger than screen
        if (mixingRect.right > _screenRect.right)
        {
            mixingRect.right = _screenRect.right;
        }
        if (mixingRect.bottom > _screenRect.bottom)
        {
            mixingRect.bottom = _screenRect.bottom;
        }
    }
    if (!EqualRect(&_mixingRect, &mixingRect))
    {
        // size changed
        CopyRect(&_mixingRect, &mixingRect);
        FillSurface(_mixingSurface, &mixingRect);
        updateAll = true;
    }

    if (_clearMixingSurface)
    {
        FillSurface(_mixingSurface, &_mixingRect);
        _clearMixingSurface = false;
        updateAll = true;
    }

    std::multimap<int, unsigned int>::reverse_iterator it;
    it = _directDrawZorder.rbegin();
    while (it != _directDrawZorder.rend())
    {
        // loop through all channels and streams in Z order
        short streamID = (it->second >> 16);
        int channel = it->second & 0x0000ffff;

        std::map<int, DirectDrawChannel*>::iterator ddIt;
        ddIt = _directDrawChannels.find(channel);
        if (ddIt != _directDrawChannels.end())
        {
            // found the channel
            DirectDrawChannel* channelObj = ddIt->second;
            if (channelObj && _mixingSurface)
            {
                if (updateAll || channelObj->IsOffScreenSurfaceUpdated(this))
                {
                    updateAll = true;
                    if (channelObj->BlitFromOffscreenBufferToMixingBuffer(
                                                                          this,
                                                                          streamID,
                                                                          _mixingSurface,
                                                                          _mixingRect,
                                                                          _demuxing)
                            != 0)
                    {
                        WEBRTC_TRACE(kTraceError, kTraceVideo,
                                     -1,
                                     "DirectDraw error BlitFromOffscreenBufferToMixingBuffer ");
                        _mixingSurface->Release();
                        _mixingSurface = NULL;
                    }
                }
            }
        }
        it++;
    }
    return 0;
}

int VideoRenderDirectDraw::BlitFromTextToMixingBuffer()
{
    if (_directDraw == NULL)
    {
        return -1;
    }
    if (!_mixingSurface)
    {
        return -1;
    }
    if (_textSettings.empty())
    {
        return 0;
    }

    HDC hdcDDSurface;
    HRESULT res = _mixingSurface->GetDC(&hdcDDSurface);
    if (res != S_OK)
    {
        return -1;
    }
    //        
    std::map<unsigned char, DirectDrawTextSettings*>::reverse_iterator it;
    it = _textSettings.rbegin();

    while (it != _textSettings.rend())
    {
        DirectDrawTextSettings* settings = it->second;
        it++;
        if (settings == NULL)
        {
            continue;
        }
        SetTextColor(hdcDDSurface, settings->_colorRefText);
        SetBkColor(hdcDDSurface, settings->_colorRefBackground);

        if (settings->_transparent)
        {
            SetBkMode(hdcDDSurface, TRANSPARENT); // do we need to call this all the time?
        }
        else
        {
            SetBkMode(hdcDDSurface, OPAQUE); // do we need to call this all the time?
        }
        RECT textRect;
        textRect.left = int(_mixingRect.right * settings->_textLeft);
        textRect.right = int(_mixingRect.right * settings->_textRight);
        textRect.top = int(_mixingRect.bottom * settings->_textTop);
        textRect.bottom = int(_mixingRect.bottom * settings->_textBottom);

        DrawTextA(hdcDDSurface, settings->_ptrText, settings->_textLength,
                  &textRect, DT_LEFT);
    }
    _mixingSurface->ReleaseDC(hdcDDSurface);
    return 0;
}

int VideoRenderDirectDraw::BlitFromBitmapBuffersToMixingBuffer()
{
    HRESULT ddrval;
    DDBLTFX ddbltfx;
    ZeroMemory(&ddbltfx, sizeof(ddbltfx));
    ddbltfx.dwSize = sizeof(ddbltfx);
    ddbltfx.dwDDFX = DDBLTFX_NOTEARING;

    if (_directDraw == NULL)
    {
        return -1; // signal that we are not ready for the change
    }

    std::map<unsigned char, DirectDrawBitmapSettings*>::reverse_iterator it;
    it = _bitmapSettings.rbegin();

    while (it != _bitmapSettings.rend())
    {
        DirectDrawBitmapSettings* settings = it->second;
        it++;
        if (settings == NULL)
        {
            continue;
        }

        // Color keying lets you set colors on a surface to be completely transparent.
        // always blit _transparentBitmapSurface last
        if (_mixingSurface && settings->_transparentBitmapSurface
                && settings->_transparentBitmapWidth
                && settings->_transparentBitmapHeight)
        {
            DWORD signal = DDBLT_WAIT | DDBLT_DDFX;
            // Set transparent color
            if (settings->_transparentBitmapColorKey)
            {
                signal |= DDBLT_KEYSRC;
                settings->_transparentBitmapSurface->SetColorKey(
                                                                 DDCKEY_SRCBLT,
                                                                 settings->_transparentBitmapColorKey);
            }

            // Now we can blt the transparent surface to another surface
            RECT srcRect;
            SetRect(&srcRect, 0, 0, settings->_transparentBitmapWidth,
                    settings->_transparentBitmapHeight);

            RECT dstRect;
            if (settings->_transparentBitmapLeft
                    != settings->_transparentBitmapRight
                    && settings->_transparentBitmapTop
                            != settings->_transparentBitmapBottom)
            {
                CopyRect(&dstRect, &_mixingRect);
                dstRect.left = (int) (dstRect.right
                        * settings->_transparentBitmapLeft);
                dstRect.right = (int) (dstRect.right
                        * settings->_transparentBitmapRight);
                dstRect.top = (int) (dstRect.bottom
                        * settings->_transparentBitmapTop);
                dstRect.bottom = (int) (dstRect.bottom
                        * settings->_transparentBitmapBottom);
            }
            else
            {

                // if left, right, top and bottom are describing one point use the original size
                CopyRect(&dstRect, &srcRect);
                POINT startp;
                startp.x = (int) (_mixingRect.right
                        * settings->_transparentBitmapLeft);
                startp.y = (int) (_mixingRect.bottom
                        * settings->_transparentBitmapTop);
                OffsetRect(&dstRect, startp.x, startp.y);

                // make sure that we blit inside our surface
                if (dstRect.bottom > _mixingRect.bottom)
                {
                    srcRect.bottom -= dstRect.bottom - _mixingRect.bottom;
                    // sanity
                    if (srcRect.bottom < 0)
                    {
                        srcRect.bottom = 0;
                    }
                    dstRect.bottom = _mixingRect.bottom;
                }
                if (dstRect.right > _mixingRect.right)
                {
                    srcRect.right -= dstRect.right - _mixingRect.right;
                    // sanity
                    if (srcRect.right < 0)
                    {
                        srcRect.right = 0;
                    }
                    dstRect.right = _mixingRect.right;
                }
            }
            // ddbltfx.dwDDFX |= DDBLTFX_MIRRORUPDOWN; //only for test requires hw support

            // wait for the  _mixingSurface to be available
            ddrval = _mixingSurface->Blt(&dstRect,
                                         settings->_transparentBitmapSurface,
                                         &srcRect, signal, &ddbltfx);
            if (ddrval == DDERR_SURFACELOST)
            {
                if (!::GetForegroundWindow())
                {
                    // no window, i.e the user have clicked CTRL+ALT+DEL
                    return 0;
                }
                // always re-creted via the SetBitmap call
                settings->_transparentBitmapSurface->Release();
                settings->_transparentBitmapSurface = NULL;

                _clearMixingSurface = true;

                if (settings->_transparentBitMap)
                {
                    WEBRTC_TRACE(kTraceInfo, kTraceVideo, -1,
                                 "DirectDraw re-set transparent bitmap");
                    settings->SetBitmap(_trace, _directDraw);
                }
            }
            else if (ddrval != DD_OK)
            {
                settings->_transparentBitmapSurface->Release();
                settings->_transparentBitmapSurface = NULL;
                WEBRTC_TRACE(
                             kTraceInfo,
                             kTraceVideo,
                             -1,
                             "DirectDraw blt error 0x%x _transparentBitmapSurface",
                             ddrval);
                return -1;
                //return VIDEO_DIRECT_DRAW_FAILURE;
            }
        }
    }
    return 0;
}

/**
 *	normal blitting
 */
int VideoRenderDirectDraw::BlitFromMixingBufferToFrontBuffer(
                                                                 bool hwndChanged,
                                                                 int& waitTime)
{
    DDBLTFX ddbltfx;
    ZeroMemory(&ddbltfx, sizeof(ddbltfx));
    ddbltfx.dwSize = sizeof(ddbltfx);
    ddbltfx.dwDDFX = DDBLTFX_NOTEARING;
    RECT rcRectDest;

    // test for changing mode
    /*    for(int i= 0; i< 6000000; i ++)
     {
     rcRectDest.left = i;
     }
     */

    if (IsRectEmpty(&_mixingRect))
    {
        // no error just nothing to blit
        return 0;
    }
    if (_mixingSurface == NULL)
    {
        // The mixing surface has probably been deleted
        // and we haven't had time to restore it yet. Wait...
        return 0;
    }
    if (_primarySurface == NULL)
    {
        int retVal = CreatePrimarySurface();
        if (retVal != 0)
        {
            // tracing done
            return retVal;
        }
    }

    // first we need to figure out where on the primary surface our window lives
    ::GetWindowRect(_hWnd, &rcRectDest);

    DWORD signal = DDBLT_WAIT | DDBLT_DDFX;

    // Set transparent color
    if (_transparentBackground)
    {
        signal |= DDBLT_KEYSRC;
        DDCOLORKEY ColorKey;
        ColorKey.dwColorSpaceLowValue = RGB(0, 0, 0);
        ColorKey.dwColorSpaceHighValue = RGB(0, 0, 0);
        _mixingSurface->SetColorKey(DDCKEY_SRCBLT, &ColorKey);
    }

    if (_renderModeWaitForCorrectScanLine)
    {
        // wait for previus draw to complete
        DWORD scanLines = 0;
        DWORD screenLines = _screenRect.bottom - 1; // scanlines start on 0
        DWORD screenLines90 = (screenLines * 9) / 10; //  % of the screen is rendered
        //waitTime=GET_TIME_IN_MS();
        waitTime = ::timeGetTime();
        HRESULT hr = _directDraw->GetScanLine(&scanLines);
        while (screenLines90 > scanLines && hr == DD_OK)
        {
            _confCritSect->Leave();
            _fullScreenWaitEvent->Wait(3);
            _confCritSect->Enter();
            if (_directDraw == NULL)
            {
                return -1;
                //return VIDEO_DIRECT_DRAW_FAILURE;
            }
            hr = _directDraw->GetScanLine(&scanLines);
        }
        //waitTime=GET_TIME_IN_MS()-waitTime;
        waitTime = ::timeGetTime() - waitTime;
    }

    HRESULT ddrval = _primarySurface->Blt(&rcRectDest, _mixingSurface,
                                          &_mixingRect, signal, &ddbltfx);
    if (ddrval == DDERR_SURFACELOST)
    {
        if (!::GetForegroundWindow())
        {
            // no window, i.e the user have clicked CTRL+ALT+DEL
            return 0;
        }
        ddrval = _primarySurface->Restore();
        if (ddrval == DD_OK) // Try again
        {
            ddrval = _primarySurface->Blt(&rcRectDest, _mixingSurface,
                                          &_mixingRect, signal, &ddbltfx);
        }
        if (ddrval != DD_OK) // If restore failed or second time blt failed. Delete the surface. It will be recreated next time.
        {
            WEBRTC_TRACE(
                         kTraceWarning,
                         kTraceVideo,
                         -1,
                         "DirectDraw failed to restore lost _primarySurface  0x%x",
                         ddrval);
            _primarySurface->Release();
            _primarySurface = NULL;
            if (_mixingSurface)
            {
                _mixingSurface->Release();
                _mixingSurface = NULL;
            }
            return -1;
            //return VIDEO_DIRECT_DRAW_FAILURE;
        }
        WEBRTC_TRACE(kTraceInfo, kTraceVideo, -1,
                     "DirectDraw restored lost _primarySurface");
    }
    else if (ddrval == DDERR_EXCEPTION)
    {
        _primarySurface->Release();
        _primarySurface = NULL;
        if (_mixingSurface)
        {
            _mixingSurface->Release();
            _mixingSurface = NULL;
        }
        WEBRTC_TRACE(kTraceError, kTraceVideo, -1,
                     "DirectDraw exception in _primarySurface");
        return -1;
        //return VIDEO_DIRECT_DRAW_FAILURE;
    }
    if (ddrval != DD_OK)
    {
        if (ddrval != 0x80004005) // Undefined error. Ignore
        {
            WEBRTC_TRACE(kTraceError, kTraceVideo, -1,
                         "DirectDraw blt error 0x%x _primarySurface", ddrval);
            return -1;
            //return VIDEO_DIRECT_DRAW_FAILURE;
        }
    }
    return 0;
}

/**
 *	fullscreen mode blitting
 */

int VideoRenderDirectDraw::WaitAndFlip(int& waitTime)
{
    if (_primarySurface == NULL)
    {
        // no trace, too much in the file
        return -1;
        //return VIDEO_DIRECT_DRAW_FAILURE;
    }
    if (_directDraw == NULL)
    {
        return -1;
        //return VIDEO_DIRECT_DRAW_FAILURE;
    }
    // wait for previus draw to complete
    DWORD scanLines = 0;
    DWORD screenLines = _screenRect.bottom - 1; // scanlines start on 0
    DWORD screenLines90 = (screenLines * 9) / 10; //  % of the screen is rendered

    //waitTime=GET_TIME_IN_MS();
    waitTime = ::timeGetTime();
    HRESULT hr = _directDraw->GetScanLine(&scanLines);
    while (screenLines90 > scanLines && hr == DD_OK)
    {
        _confCritSect->Leave();
        _fullScreenWaitEvent->Wait(3);
        _confCritSect->Enter();
        if (_directDraw == NULL)
        {
            return -1;
            //return VIDEO_DIRECT_DRAW_FAILURE;
        }
        hr = _directDraw->GetScanLine(&scanLines);
    }
    //waitTime=GET_TIME_IN_MS()-waitTime;    
    waitTime = ::timeGetTime() - waitTime;
    if (screenLines > scanLines)
    {
        // this function sucks a lot of the CPU... but it's worth it
        _directDraw->WaitForVerticalBlank(DDWAITVB_BLOCKBEGIN, NULL);
    }

    // schedule a flip
    HRESULT ddrval = _primarySurface->Flip(NULL, DDFLIP_WAIT); // schedule flip DDFLIP_WAIT
    if (ddrval == DDERR_SURFACELOST)
    {
        if (!::GetForegroundWindow())
        {
            // no window, i.e the user have clicked CTRL+ALT+DEL
            return 0;
        }
        //if(::IsIconic(_hWnd))
        //{
        // need to do this before Restore
        //WEBRTC_TRACE(kTraceInfo, kTraceVideo, -1, "DirectDraw our window is an icon maximize it ");
        // When the full screen window is switched out by ALT-TAB or ALT-CTRL-DEL-TASKMANAGER,
        // this call will hang the app. Remove it to fix the problem.
        // FIXME:
        // 1) Why we want to active and max the window when it was minimized?
        // 2) Why this is needed before restore? We didn't do that in non full screen mode.
        //::ShowWindow(_hWnd, SW_SHOWMAXIMIZED);
        //}
        ddrval = _primarySurface->Restore();
        if (ddrval != DD_OK)
        {
            WEBRTC_TRACE(
                         kTraceWarning,
                         kTraceVideo,
                         -1,
                         "DirectDraw failed to restore _primarySurface, in flip, 0x%x",
                         ddrval);
            return -1;
            //return VIDEO_DIRECT_DRAW_FAILURE;
        }
        WEBRTC_TRACE(kTraceInfo, kTraceVideo, -1,
                     "DirectDraw restore _primarySurface in flip");

    }
    else if (ddrval != DD_OK)
    {
        return -1;
        //return VIDEO_DIRECT_DRAW_FAILURE;
    }
    return 0;
}

int VideoRenderDirectDraw::BlitFromMixingBufferToBackBuffer()
{
    if (_backSurface == NULL)
    {
        return -1;
        //return VIDEO_DIRECT_DRAW_FAILURE;
    }
    if (IsRectEmpty(&_mixingRect))
    {
        // nothing to blit
        return 0;
    }
    DDBLTFX ddbltfx;
    ZeroMemory(&ddbltfx, sizeof(ddbltfx));
    ddbltfx.dwSize = sizeof(ddbltfx);
    ddbltfx.dwDDFX = DDBLTFX_NOTEARING;

    // wait for the _backSurface to be available
    HRESULT ddrval = _backSurface->Blt(&_screenRect, _mixingSurface,
                                       &_mixingRect, DDBLT_WAIT | DDBLT_DDFX,
                                       &ddbltfx);
    if (ddrval == DDERR_SURFACELOST)
    {
        if (!::GetForegroundWindow())
        {
            // no window, i.e the user have clicked CTRL+ALT+DEL
            return 0;
        }
        //if(::IsIconic(_hWnd))
        //{
        // need to do this before Restore
        //WEBRTC_TRACE(kTraceInfo, kTraceVideo, -1, "DirectDraw our window is an icon maximize it ");
        //WEBRTC_TRACE(kTraceInfo, kTraceVideo, -1, "DirectDraw show our window is an icon maximize it ");
        // When the full screen window is switch out by ALT-TAB or ALT-CTRL-DEL-TASKMANAGER,
        // this call will hang the app. Remove it to fix the problem.
        // FIXME:
        // 1) Why we want to active and max the window when it was minimized?
        // 2) Why this is needed before restore? We didn't do that in non full screen mode.
        //::ShowWindow(_hWnd, SW_SHOWMAXIMIZED);
        //}
        ddrval = _primarySurface->Restore();
        if (ddrval != DD_OK)
        {
            WEBRTC_TRACE(kTraceWarning, kTraceVideo, -1,
                         "DirectDraw failed to restore _primarySurface");
            return -1;
            //return VIDEO_DIRECT_DRAW_FAILURE;
        }
        WEBRTC_TRACE(kTraceInfo, kTraceVideo, -1,
                     "DirectDraw restored _primarySurface");

        _clearMixingSurface = true;

    }
    else if (ddrval != DD_OK)
    {
        WEBRTC_TRACE(kTraceError, kTraceVideo, -1,
                     "DirectDraw blt error 0x%x _backSurface", ddrval);
        return -1;
        //return VIDEO_DIRECT_DRAW_FAILURE;
    }
    return 0;
}

/*
 Saving the code for using a clip list instead of HWND, problem was that other transparent
 HWNDs caused us not to update an area or that we painted in other HWNDs area.

 RECT hWndRect;
 ::GetWindowRect(_hWnd, &hWndRect);

 LPRGNDATA lpClipList = (LPRGNDATA)malloc(sizeof(RGNDATAHEADER) + sizeof(RECT));

 // now fill out all the structure fields
 memcpy(lpClipList->Buffer, &hWndRect, sizeof(RECT));

 ::CopyRect(&(lpClipList->rdh.rcBound), &hWndRect);
 lpClipList->rdh.dwSize = sizeof(RGNDATAHEADER);
 lpClipList->rdh.iType = RDH_RECTANGLES;
 lpClipList->rdh.nCount = 1;
 lpClipList->rdh.nRgnSize = sizeof(RECT) * lpClipList->rdh.nCount;
 ddrval= _directDrawClipper->SetClipList(lpClipList, 0);

 void Visible(HWND hwnd, HRGN &hRgn)
 {
 if (!IsWindowVisible(hwnd))      // If the window is visible
 {
 if(CombineRgn(hRgn, hRgn, hRgn, RGN_XOR) == NULLREGION)
 {
 return;
 }
 }
 // Gets the topmost window
 HWND hWnd=GetTopWindow(NULL);
 while (hWnd != NULL && hWnd != hwnd)  // If the window is above in Z-order
 {
 if (IsWindowVisible(hWnd))      // If the window is visible
 {
 RECT Rect;
 // Gets window dimension
 GetWindowRect(hWnd, &Rect);
 // Creates a region corresponding to the window
 if(Rect.left > 0) // test fo rnow
 {
 HRGN hrgnWnd = CreateRectRgn(Rect.left, Rect.top, Rect.right, Rect.bottom);
 //                int err = GetUpdateRgn(hWnd, hrgnWnd, FALSE);
 // Creates a region corresponding to region not overlapped
 if(CombineRgn(hRgn, hRgn, hrgnWnd, RGN_DIFF) == COMPLEXREGION)
 {
 int a = 0;
 }
 DeleteObject(hrgnWnd);
 }
 }
 // Loops through all windows till the specified window
 hWnd = GetWindow(hWnd, GW_HWNDNEXT);
 }

 HRGN region;
 region = CreateRectRgn(0, 0, 500, 500);

 // Get the affected region
 //    if (GetUpdateRgn(_hWnd, region, FALSE) != ERROR)
 HDC dc = GetDC(_hWnd);
 if(GetClipRgn(dc, region) > 0)
 {
 int buffsize;
 UINT x;
 RGNDATA *buff;
 POINT TopLeft;

 // Get the top-left point of the client area
 TopLeft.x = 0;
 TopLeft.y = 0;
 if (!ClientToScreen(_hWnd, &TopLeft))
 {
 int a = 0;
 }


 // Get the size of buffer required
 buffsize = GetRegionData(region, 0, 0);
 if (buffsize != 0)
 {
 buff = (RGNDATA *) new BYTE [buffsize];
 if (buff == NULL)
 {
 int a = 0;
 }

 // Now get the region data
 if(GetRegionData(region, buffsize, buff))
 {
 if(buff->rdh.nCount > 0)
 {
 ::OffsetRect(&(buff->rdh.rcBound), TopLeft.x, TopLeft.y);
 for (x=0; x<(buff->rdh.nCount); x++)
 {
 RECT *urect = (RECT *) (((BYTE *) buff) + sizeof(RGNDATAHEADER) + (x * sizeof(RECT)));
 ::OffsetRect(urect, TopLeft.x, TopLeft.y);
 char logStr[256];
 _snprintf(logStr,256, "rect T:%d L:%d B:%d R:%d\n",urect->top, urect->left, urect->bottom, urect->right);
 OutputDebugString(logStr);

 }
 OutputDebugString("\n");
 _directDrawClipper->SetClipList(buff, 0);
 }
 LPRGNDATA lpClipList = (LPRGNDATA)malloc(sizeof(RGNDATAHEADER) + sizeof(RECT) * buff->rdh.nCount);
 if(buff->rdh.nCount > 0)
 {
 _directDrawClipper->SetClipList(lpClipList, 0);

 lpClipList->
 DWORD size = sizeof(RGNDATAHEADER) + sizeof(RECT)* buff->rdh.nCount;
 lpClipList->rdh.dwSize = sizeof(RGNDATAHEADER);
 lpClipList->rdh.iType = RDH_RECTANGLES;
 lpClipList->rdh.nCount = 1;

 HRESULT ddrval1 = _directDrawClipper->GetClipList(NULL, lpClipList, &size);
 memcpy(lpClipList->Buffer, &rcRectDest, sizeof(RECT));
 ::CopyRect(&(lpClipList->rdh.rcBound), &rcRectDest);
 _directDrawClipper->SetClipList(lpClipList, 0);
 }                    }

 for (x=0; x<(buff->rdh.nCount); x++)
 {
 // Obtain the rectangles from the list
 RECT *urect = (RECT *) (((BYTE *) buff) + sizeof(RGNDATAHEADER) + (x * sizeof(RECT)));
 int a = 0;

 }
 delete lpClipList;
 }
 delete buff;
 }
 }
 */
/*
 void VideoRenderDirectDraw::Wait()
 {
 // wait for previus draw to complete
 int count = 0;
 DWORD scanLines = 0;
 DWORD screenLines = _screenRect.bottom -1; // scanlines start on 0
 DWORD screenLines75 = (screenLines*3)/4; //  % of the screen is rendered
 HRESULT hr = DD_OK;
 if(_directDraw == NULL)
 {
 return;
 }
 hr =_directDraw->GetScanLine(&scanLines);
 while ( screenLines75 > scanLines && hr == DD_OK)
 {
 //   		_confCritSect->Leave();
 _screenEvent->Wait(10);
 //      _confCritSect->Enter();
 if(_directDraw == NULL)
 {
 return;
 }
 hr = _directDraw->GetScanLine(&scanLines);
 }
 }
 */

WebRtc_Word32 VideoRenderDirectDraw::StartRender()
{
    WEBRTC_TRACE(kTraceError, kTraceVideo, -1, "Not supported.");
    return 0;
}

WebRtc_Word32 VideoRenderDirectDraw::StopRender()
{
    WEBRTC_TRACE(kTraceError, kTraceVideo, -1, "Not supported.");
    return 0;
}

WebRtc_Word32 VideoRenderDirectDraw::ChangeWindow(void* window)
{
    WEBRTC_TRACE(kTraceError, kTraceVideo, -1, "Not supported.");
    return -1;
}

} //namespace webrtc

