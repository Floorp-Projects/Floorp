/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/video_render/test/testAPI/testAPI.h"

#include <stdio.h>

#if defined(_WIN32)
#include <tchar.h>
#include <windows.h>
#include <assert.h>
#include <fstream>
#include <iostream>
#include <string>
#include <windows.h>
#include <ddraw.h>

#elif defined(WEBRTC_LINUX) && !defined(WEBRTC_ANDROID)

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <iostream>
#include <sys/time.h>

#endif

#include "webrtc/common_types.h"
#include "webrtc/modules/interface/module_common_types.h"
#include "webrtc/modules/utility/interface/process_thread.h"
#include "webrtc/modules/video_render/include/video_render.h"
#include "webrtc/modules/video_render/include/video_render_defines.h"
#include "webrtc/system_wrappers/interface/sleep.h"
#include "webrtc/system_wrappers/interface/tick_util.h"
#include "webrtc/system_wrappers/interface/trace.h"

using namespace webrtc;

void GetTestVideoFrame(I420VideoFrame* frame,
                       uint8_t startColor);
int TestSingleStream(VideoRender* renderModule);
int TestFullscreenStream(VideoRender* &renderModule,
                         void* window,
                         const VideoRenderType videoRenderType);
int TestBitmapText(VideoRender* renderModule);
int TestMultipleStreams(VideoRender* renderModule);
int TestExternalRender(VideoRender* renderModule);

#define TEST_FRAME_RATE 30
#define TEST_TIME_SECOND 5
#define TEST_FRAME_NUM (TEST_FRAME_RATE*TEST_TIME_SECOND)
#define TEST_STREAM0_START_COLOR 0
#define TEST_STREAM1_START_COLOR 64
#define TEST_STREAM2_START_COLOR 128
#define TEST_STREAM3_START_COLOR 192

#if defined(WEBRTC_LINUX)

#define GET_TIME_IN_MS timeGetTime()

unsigned long timeGetTime()
{
    struct timeval tv;
    struct timezone tz;
    unsigned long val;

    gettimeofday(&tv, &tz);
    val= tv.tv_sec*1000+ tv.tv_usec/1000;
    return(val);
}

#elif defined(WEBRTC_MAC)

#include <unistd.h>

#define GET_TIME_IN_MS timeGetTime()

unsigned long timeGetTime()
{
    return 0;
}

#else

#define GET_TIME_IN_MS ::timeGetTime()

#endif

using namespace std;

#if defined(_WIN32)
LRESULT CALLBACK WebRtcWinProc( HWND hWnd,UINT uMsg,WPARAM wParam,LPARAM lParam)
{
    switch(uMsg)
    {
        case WM_DESTROY:
        break;
        case WM_COMMAND:
        break;
    }
    return DefWindowProc(hWnd,uMsg,wParam,lParam);
}

int WebRtcCreateWindow(HWND &hwndMain,int winNum, int width, int height)
{
    HINSTANCE hinst = GetModuleHandle(0);
    WNDCLASSEX wcx;
    wcx.hInstance = hinst;
    wcx.lpszClassName = TEXT("VideoRenderTest");
    wcx.lpfnWndProc = (WNDPROC)WebRtcWinProc;
    wcx.style = CS_DBLCLKS;
    wcx.hIcon = LoadIcon (NULL, IDI_APPLICATION);
    wcx.hIconSm = LoadIcon (NULL, IDI_APPLICATION);
    wcx.hCursor = LoadCursor (NULL, IDC_ARROW);
    wcx.lpszMenuName = NULL;
    wcx.cbSize = sizeof (WNDCLASSEX);
    wcx.cbClsExtra = 0;
    wcx.cbWndExtra = 0;
    wcx.hbrBackground = GetSysColorBrush(COLOR_3DFACE);

    // Register our window class with the operating system.
    // If there is an error, exit program.
    if ( !RegisterClassEx (&wcx) )
    {
        MessageBox( 0, TEXT("Failed to register window class!"),TEXT("Error!"), MB_OK|MB_ICONERROR );
        return 0;
    }

    // Create the main window.
    hwndMain = CreateWindowEx(
            0, // no extended styles
            TEXT("VideoRenderTest"), // class name
            TEXT("VideoRenderTest Window"), // window name
            WS_OVERLAPPED |WS_THICKFRAME, // overlapped window
            800, // horizontal position
            0, // vertical position
            width, // width
            height, // height
            (HWND) NULL, // no parent or owner window
            (HMENU) NULL, // class menu used
            hinst, // instance handle
            NULL); // no window creation data

    if (!hwndMain)
        return -1;

    // Show the window using the flag specified by the program
    // that started the application, and send the application
    // a WM_PAINT message.

    ShowWindow(hwndMain, SW_SHOWDEFAULT);
    UpdateWindow(hwndMain);
    return 0;
}

#elif defined(WEBRTC_LINUX) && !defined(WEBRTC_ANDROID)

int WebRtcCreateWindow(Window *outWindow, Display **outDisplay, int winNum, int width, int height) // unsigned char* title, int titleLength)

{
    int screen, xpos = 10, ypos = 10;
    XEvent evnt;
    XSetWindowAttributes xswa; // window attribute struct
    XVisualInfo vinfo; // screen visual info struct
    unsigned long mask; // attribute mask

    // get connection handle to xserver
    Display* _display = XOpenDisplay( NULL );

    // get screen number
    screen = DefaultScreen(_display);

    // put desired visual info for the screen in vinfo
    if( XMatchVisualInfo(_display, screen, 24, TrueColor, &vinfo) != 0 )
    {
        //printf( "Screen visual info match!\n" );
    }

    // set window attributes
    xswa.colormap = XCreateColormap(_display, DefaultRootWindow(_display), vinfo.visual, AllocNone);
    xswa.event_mask = StructureNotifyMask | ExposureMask;
    xswa.background_pixel = 0;
    xswa.border_pixel = 0;

    // value mask for attributes
    mask = CWBackPixel | CWBorderPixel | CWColormap | CWEventMask;

    switch( winNum )
    {
        case 0:
        xpos = 200;
        ypos = 200;
        break;
        case 1:
        xpos = 300;
        ypos = 200;
        break;
        default:
        break;
    }

    // create a subwindow for parent (defroot)
    Window _window = XCreateWindow(_display, DefaultRootWindow(_display),
            xpos, ypos,
            width,
            height,
            0, vinfo.depth,
            InputOutput,
            vinfo.visual,
            mask, &xswa);

    // Set window name
    if( winNum == 0 )
    {
        XStoreName(_display, _window, "VE MM Local Window");
        XSetIconName(_display, _window, "VE MM Local Window");
    }
    else if( winNum == 1 )
    {
        XStoreName(_display, _window, "VE MM Remote Window");
        XSetIconName(_display, _window, "VE MM Remote Window");
    }

    // make x report events for mask
    XSelectInput(_display, _window, StructureNotifyMask);

    // map the window to the display
    XMapWindow(_display, _window);

    // wait for map event
    do
    {
        XNextEvent(_display, &evnt);
    }
    while (evnt.type != MapNotify || evnt.xmap.event != _window);

    *outWindow = _window;
    *outDisplay = _display;

    return 0;
}
#endif  // LINUX

// Note: Mac code is in testApi_mac.mm.

class MyRenderCallback: public VideoRenderCallback
{
public:
    MyRenderCallback() :
        _cnt(0)
    {
    }
    ;
    ~MyRenderCallback()
    {
    }
    ;
    virtual int32_t RenderFrame(const uint32_t streamId,
                                I420VideoFrame& videoFrame)
    {
        _cnt++;
        if (_cnt % 100 == 0)
        {
            printf("Render callback %d \n",_cnt);
        }
        return 0;
    }
    int32_t _cnt;
};

void GetTestVideoFrame(I420VideoFrame* frame,
                       uint8_t startColor) {
    // changing color
    static uint8_t color = startColor;

    memset(frame->buffer(kYPlane), color, frame->allocated_size(kYPlane));
    memset(frame->buffer(kUPlane), color, frame->allocated_size(kUPlane));
    memset(frame->buffer(kVPlane), color, frame->allocated_size(kVPlane));

    ++color;
}

int TestSingleStream(VideoRender* renderModule) {
    int error = 0;
    // Add settings for a stream to render
    printf("Add stream 0 to entire window\n");
    const int streamId0 = 0;
    VideoRenderCallback* renderCallback0 = renderModule->AddIncomingRenderStream(streamId0, 0, 0.0f, 0.0f, 1.0f, 1.0f);
    assert(renderCallback0 != NULL);

    printf("Start render\n");
    error = renderModule->StartRender(streamId0);
    if (error != 0) {
      // TODO(phoglund): This test will not work if compiled in release mode.
      // This rather silly construct here is to avoid compilation errors when
      // compiling in release. Release => no asserts => unused 'error' variable.
      assert(false);
    }

    // Loop through an I420 file and render each frame
    const int width = 352;
    const int half_width = (width + 1) / 2;
    const int height = 288;

    I420VideoFrame videoFrame0;
    videoFrame0.CreateEmptyFrame(width, height, width, half_width, half_width);

    const uint32_t renderDelayMs = 500;

    for (int i=0; i<TEST_FRAME_NUM; i++) {
        GetTestVideoFrame(&videoFrame0, TEST_STREAM0_START_COLOR);
        // Render this frame with the specified delay
        videoFrame0.set_render_time_ms(TickTime::MillisecondTimestamp()
                                       + renderDelayMs);
        renderCallback0->RenderFrame(streamId0, videoFrame0);
        SleepMs(1000/TEST_FRAME_RATE);
    }


    // Shut down
    printf("Closing...\n");
    error = renderModule->StopRender(streamId0);
    assert(error == 0);

    error = renderModule->DeleteIncomingRenderStream(streamId0);
    assert(error == 0);

    return 0;
}

int TestFullscreenStream(VideoRender* &renderModule,
                         void* window,
                         const VideoRenderType videoRenderType) {
    VideoRender::DestroyVideoRender(renderModule);
    renderModule = VideoRender::CreateVideoRender(12345, window, true, videoRenderType);

    TestSingleStream(renderModule);

    VideoRender::DestroyVideoRender(renderModule);
    renderModule = VideoRender::CreateVideoRender(12345, window, false, videoRenderType);

    return 0;
}

int TestBitmapText(VideoRender* renderModule) {
#if defined(WIN32)

    int error = 0;
    // Add settings for a stream to render
    printf("Add stream 0 to entire window\n");
    const int streamId0 = 0;
    VideoRenderCallback* renderCallback0 = renderModule->AddIncomingRenderStream(streamId0, 0, 0.0f, 0.0f, 1.0f, 1.0f);
    assert(renderCallback0 != NULL);

    printf("Adding Bitmap\n");
    DDCOLORKEY ColorKey; // black
    ColorKey.dwColorSpaceHighValue = RGB(0, 0, 0);
    ColorKey.dwColorSpaceLowValue = RGB(0, 0, 0);
    HBITMAP hbm = (HBITMAP)LoadImage(NULL,
                                     (LPCTSTR)_T("renderStartImage.bmp"),
                                     IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE);
    renderModule->SetBitmap(hbm, 0, &ColorKey, 0.0f, 0.0f, 0.3f,
                             0.3f);

    printf("Adding Text\n");
    renderModule->SetText(1, (uint8_t*) "WebRtc Render Demo App", 20,
                           RGB(255, 0, 0), RGB(0, 0, 0), 0.25f, 0.1f, 1.0f,
                           1.0f);

    printf("Start render\n");
    error = renderModule->StartRender(streamId0);
    assert(error == 0);

    // Loop through an I420 file and render each frame
    const int width = 352;
    const int half_width = (width + 1) / 2;
    const int height = 288;

    I420VideoFrame videoFrame0;
    videoFrame0.CreateEmptyFrame(width, height, width, half_width, half_width);

    const uint32_t renderDelayMs = 500;

    for (int i=0; i<TEST_FRAME_NUM; i++) {
        GetTestVideoFrame(&videoFrame0, TEST_STREAM0_START_COLOR);
        // Render this frame with the specified delay
        videoFrame0.set_render_time_ms(TickTime::MillisecondTimestamp() +
                                       renderDelayMs);
        renderCallback0->RenderFrame(streamId0, videoFrame0);
        SleepMs(1000/TEST_FRAME_RATE);
    }
    // Sleep and let all frames be rendered before closing
    SleepMs(renderDelayMs*2);


    // Shut down
    printf("Closing...\n");
    ColorKey.dwColorSpaceHighValue = RGB(0,0,0);
    ColorKey.dwColorSpaceLowValue = RGB(0,0,0);
    renderModule->SetBitmap(NULL, 0, &ColorKey, 0.0f, 0.0f, 0.0f, 0.0f);
    renderModule->SetText(1, NULL, 20, RGB(255,255,255),
                    RGB(0,0,0), 0.0f, 0.0f, 0.0f, 0.0f);

    error = renderModule->StopRender(streamId0);
    assert(error == 0);

    error = renderModule->DeleteIncomingRenderStream(streamId0);
    assert(error == 0);
#endif

    return 0;
}

int TestMultipleStreams(VideoRender* renderModule) {
    int error = 0;

    // Add settings for a stream to render
    printf("Add stream 0\n");
    const int streamId0 = 0;
    VideoRenderCallback* renderCallback0 =
        renderModule->AddIncomingRenderStream(streamId0, 0, 0.0f, 0.0f, 0.45f, 0.45f);
    assert(renderCallback0 != NULL);
    printf("Add stream 1\n");
    const int streamId1 = 1;
    VideoRenderCallback* renderCallback1 =
        renderModule->AddIncomingRenderStream(streamId1, 0, 0.55f, 0.0f, 1.0f, 0.45f);
    assert(renderCallback1 != NULL);
    printf("Add stream 2\n");
    const int streamId2 = 2;
    VideoRenderCallback* renderCallback2 =
        renderModule->AddIncomingRenderStream(streamId2, 0, 0.0f, 0.55f, 0.45f, 1.0f);
    assert(renderCallback2 != NULL);
    printf("Add stream 3\n");
    const int streamId3 = 3;
    VideoRenderCallback* renderCallback3 =
        renderModule->AddIncomingRenderStream(streamId3, 0, 0.55f, 0.55f, 1.0f, 1.0f);
    assert(renderCallback3 != NULL);
    error = renderModule->StartRender(streamId0);
    if (error != 0) {
      // TODO(phoglund): This test will not work if compiled in release mode.
      // This rather silly construct here is to avoid compilation errors when
      // compiling in release. Release => no asserts => unused 'error' variable.
      assert(false);
    }
    error = renderModule->StartRender(streamId1);
    assert(error == 0);
    error = renderModule->StartRender(streamId2);
    assert(error == 0);
    error = renderModule->StartRender(streamId3);
    assert(error == 0);

    // Loop through an I420 file and render each frame
    const int width = 352;
    const int half_width = (width + 1) / 2;
    const int height = 288;

    I420VideoFrame videoFrame0;
    videoFrame0.CreateEmptyFrame(width, height, width, half_width, half_width);
    I420VideoFrame videoFrame1;
    videoFrame1.CreateEmptyFrame(width, height, width, half_width, half_width);
    I420VideoFrame videoFrame2;
    videoFrame2.CreateEmptyFrame(width, height, width, half_width, half_width);
    I420VideoFrame videoFrame3;
    videoFrame3.CreateEmptyFrame(width, height, width, half_width, half_width);

    const uint32_t renderDelayMs = 500;

    // Render frames with the specified delay.
    for (int i=0; i<TEST_FRAME_NUM; i++) {
      GetTestVideoFrame(&videoFrame0, TEST_STREAM0_START_COLOR);

      videoFrame0.set_render_time_ms(TickTime::MillisecondTimestamp() +
                                     renderDelayMs);
      renderCallback0->RenderFrame(streamId0, videoFrame0);

      GetTestVideoFrame(&videoFrame1, TEST_STREAM1_START_COLOR);
      videoFrame1.set_render_time_ms(TickTime::MillisecondTimestamp() +
                                     renderDelayMs);
      renderCallback1->RenderFrame(streamId1, videoFrame1);

      GetTestVideoFrame(&videoFrame2,  TEST_STREAM2_START_COLOR);
      videoFrame2.set_render_time_ms(TickTime::MillisecondTimestamp() +
                                     renderDelayMs);
      renderCallback2->RenderFrame(streamId2, videoFrame2);

      GetTestVideoFrame(&videoFrame3, TEST_STREAM3_START_COLOR);
      videoFrame3.set_render_time_ms(TickTime::MillisecondTimestamp() +
                                     renderDelayMs);
      renderCallback3->RenderFrame(streamId3, videoFrame3);

      SleepMs(1000/TEST_FRAME_RATE);
    }

    // Shut down
    printf("Closing...\n");
    error = renderModule->StopRender(streamId0);
    assert(error == 0);
    error = renderModule->DeleteIncomingRenderStream(streamId0);
    assert(error == 0);
    error = renderModule->StopRender(streamId1);
    assert(error == 0);
    error = renderModule->DeleteIncomingRenderStream(streamId1);
    assert(error == 0);
    error = renderModule->StopRender(streamId2);
    assert(error == 0);
    error = renderModule->DeleteIncomingRenderStream(streamId2);
    assert(error == 0);
    error = renderModule->StopRender(streamId3);
    assert(error == 0);
    error = renderModule->DeleteIncomingRenderStream(streamId3);
    assert(error == 0);

    return 0;
}

int TestExternalRender(VideoRender* renderModule) {
    int error = 0;
    MyRenderCallback *externalRender = new MyRenderCallback();

    const int streamId0 = 0;
    VideoRenderCallback* renderCallback0 =
        renderModule->AddIncomingRenderStream(streamId0, 0, 0.0f, 0.0f,
                                                   1.0f, 1.0f);
    assert(renderCallback0 != NULL);
    error = renderModule->AddExternalRenderCallback(streamId0, externalRender);
    if (error != 0) {
      // TODO(phoglund): This test will not work if compiled in release mode.
      // This rather silly construct here is to avoid compilation errors when
      // compiling in release. Release => no asserts => unused 'error' variable.
      assert(false);
    }

    error = renderModule->StartRender(streamId0);
    assert(error == 0);

    const int width = 352;
    const int half_width = (width + 1) / 2;
    const int height = 288;
    I420VideoFrame videoFrame0;
    videoFrame0.CreateEmptyFrame(width, height, width, half_width, half_width);

    const uint32_t renderDelayMs = 500;
    int frameCount = TEST_FRAME_NUM;
    for (int i=0; i<frameCount; i++) {
        videoFrame0.set_render_time_ms(TickTime::MillisecondTimestamp() +
                                       renderDelayMs);
        renderCallback0->RenderFrame(streamId0, videoFrame0);
        SleepMs(33);
    }

    // Sleep and let all frames be rendered before closing
    SleepMs(2*renderDelayMs);

    // Shut down
    printf("Closing...\n");
    error = renderModule->StopRender(streamId0);
    assert(error == 0);
    error = renderModule->DeleteIncomingRenderStream(streamId0);
    assert(error == 0);
    assert(frameCount == externalRender->_cnt);

    delete externalRender;
    externalRender = NULL;

    return 0;
}

void RunVideoRenderTests(void* window, VideoRenderType windowType) {
    int myId = 12345;

    // Create the render module
    printf("Create render module\n");
    VideoRender* renderModule = NULL;
    renderModule = VideoRender::CreateVideoRender(myId,
                                                  window,
                                                  false,
                                                  windowType);
    assert(renderModule != NULL);

    // ##### Test single stream rendering ####
    printf("#### TestSingleStream ####\n");
    if (TestSingleStream(renderModule) != 0) {
        printf ("TestSingleStream failed\n");
    }

    // ##### Test fullscreen rendering ####
    printf("#### TestFullscreenStream ####\n");
    if (TestFullscreenStream(renderModule, window, windowType) != 0) {
        printf ("TestFullscreenStream failed\n");
    }

    // ##### Test bitmap and text ####
    printf("#### TestBitmapText ####\n");
    if (TestBitmapText(renderModule) != 0) {
        printf ("TestBitmapText failed\n");
    }

    // ##### Test multiple streams ####
    printf("#### TestMultipleStreams ####\n");
    if (TestMultipleStreams(renderModule) != 0) {
        printf ("TestMultipleStreams failed\n");
    }

    // ##### Test multiple streams ####
    printf("#### TestExternalRender ####\n");
    if (TestExternalRender(renderModule) != 0) {
        printf ("TestExternalRender failed\n");
    }

    delete renderModule;
    renderModule = NULL;

    printf("VideoRender unit tests passed.\n");
}

// Note: The Mac main is implemented in testApi_mac.mm.
#if defined(_WIN32)
int _tmain(int argc, _TCHAR* argv[])
#elif defined(WEBRTC_LINUX) && !defined(WEBRTC_ANDROID)
int main(int argc, char* argv[])
#endif
#if !defined(WEBRTC_MAC) && !defined(WEBRTC_ANDROID)
{
    // Create a window for testing.
    void* window = NULL;
#if defined (_WIN32)
    HWND testHwnd;
    WebRtcCreateWindow(testHwnd, 0, 352, 288);
    window = (void*)testHwnd;
    VideoRenderType windowType = kRenderWindows;
#elif defined(WEBRTC_LINUX)
    Window testWindow;
    Display* display;
    WebRtcCreateWindow(&testWindow, &display, 0, 352, 288);
    VideoRenderType windowType = kRenderX11;
    window = (void*)testWindow;
#endif // WEBRTC_LINUX

    RunVideoRenderTests(window, windowType);
    return 0;
}
#endif  // !WEBRTC_MAC
