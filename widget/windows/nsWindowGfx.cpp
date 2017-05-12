/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * nsWindowGfx - Painting and aceleration.
 */

/**************************************************************
 **************************************************************
 **
 ** BLOCK: Includes
 **
 ** Include headers.
 **
 **************************************************************
 **************************************************************/

#include "mozilla/dom/ContentParent.h"
#include "mozilla/plugins/PluginInstanceParent.h"
using mozilla::plugins::PluginInstanceParent;

#include "nsWindowGfx.h"
#include "nsAppRunner.h"
#include <windows.h>
#include "gfxEnv.h"
#include "gfxImageSurface.h"
#include "gfxUtils.h"
#include "gfxWindowsSurface.h"
#include "gfxWindowsPlatform.h"
#include "gfxDWriteFonts.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/DataSurfaceHelpers.h"
#include "mozilla/gfx/Tools.h"
#include "mozilla/RefPtr.h"
#include "mozilla/UniquePtrExtensions.h"
#include "nsGfxCIID.h"
#include "gfxContext.h"
#include "prmem.h"
#include "WinUtils.h"
#include "nsIWidgetListener.h"
#include "mozilla/Unused.h"
#include "nsDebug.h"
#include "nsIXULRuntime.h"

#include "mozilla/gfx/GPUProcessManager.h"
#include "mozilla/layers/CompositorBridgeParent.h"
#include "mozilla/layers/CompositorBridgeChild.h"
#include "ClientLayerManager.h"
#include "WinCompositorWidget.h"

#include "nsUXThemeData.h"
#include "nsUXThemeConstants.h"

using namespace mozilla;
using namespace mozilla::gfx;
using namespace mozilla::layers;
using namespace mozilla::widget;
using namespace mozilla::plugins;

/**************************************************************
 **************************************************************
 **
 ** BLOCK: Variables
 **
 ** nsWindow Class static initializations and global variables.
 **
 **************************************************************
 **************************************************************/

/**************************************************************
 *
 * SECTION: nsWindow statics
 *
 **************************************************************/

static UniquePtr<uint8_t[]> sSharedSurfaceData;
static IntSize             sSharedSurfaceSize;

struct IconMetrics {
  int32_t xMetric;
  int32_t yMetric;
  int32_t defaultSize;
};

// Corresponds 1:1 to the IconSizeType enum
static IconMetrics sIconMetrics[] = {
  {SM_CXSMICON, SM_CYSMICON, 16}, // small icon
  {SM_CXICON,   SM_CYICON,   32}  // regular icon
};

/**************************************************************
 **************************************************************
 **
 ** BLOCK: nsWindow impl.
 **
 ** Paint related nsWindow methods.
 **
 **************************************************************
 **************************************************************/

// GetRegionToPaint returns the invalidated region that needs to be painted
nsIntRegion nsWindow::GetRegionToPaint(bool aForceFullRepaint,
                                       PAINTSTRUCT ps, HDC aDC)
{
  if (aForceFullRepaint) {
    RECT paintRect;
    ::GetClientRect(mWnd, &paintRect);
    return nsIntRegion(WinUtils::ToIntRect(paintRect));
  }

  HRGN paintRgn = ::CreateRectRgn(0, 0, 0, 0);
  if (paintRgn != nullptr) {
    int result = GetRandomRgn(aDC, paintRgn, SYSRGN);
    if (result == 1) {
      POINT pt = {0,0};
      ::MapWindowPoints(nullptr, mWnd, &pt, 1);
      ::OffsetRgn(paintRgn, pt.x, pt.y);
    }
    nsIntRegion rgn(WinUtils::ConvertHRGNToRegion(paintRgn));
    ::DeleteObject(paintRgn);
    return rgn;
  }
  return nsIntRegion(WinUtils::ToIntRect(ps.rcPaint));
}

#define WORDSSIZE(x) ((x).width * (x).height)
static bool
EnsureSharedSurfaceSize(IntSize size)
{
  IntSize screenSize;
  screenSize.height = GetSystemMetrics(SM_CYSCREEN);
  screenSize.width = GetSystemMetrics(SM_CXSCREEN);

  if (WORDSSIZE(screenSize) > WORDSSIZE(size))
    size = screenSize;

  if (WORDSSIZE(screenSize) < WORDSSIZE(size))
    NS_WARNING("Trying to create a shared surface larger than the screen");

  if (!sSharedSurfaceData || (WORDSSIZE(size) > WORDSSIZE(sSharedSurfaceSize))) {
    sSharedSurfaceSize = size;
    sSharedSurfaceData =
      MakeUniqueFallible<uint8_t[]>(WORDSSIZE(sSharedSurfaceSize) * 4);
  }

  return !sSharedSurfaceData;
}

nsIWidgetListener* nsWindow::GetPaintListener()
{
  if (mDestroyCalled)
    return nullptr;
  return mAttachedWidgetListener ? mAttachedWidgetListener : mWidgetListener;
}

void nsWindow::ForcePresent()
{
  if (mResizeState != RESIZING) {
    if (CompositorBridgeChild* remoteRenderer = GetRemoteRenderer()) {
      remoteRenderer->SendForcePresent();
    }
  }
}

bool nsWindow::OnPaint(HDC aDC, uint32_t aNestingLevel)
{
  // We never have reentrant paint events, except when we're running our RPC
  // windows event spin loop. If we don't trap for this, we'll try to paint,
  // but view manager will refuse to paint the surface, resulting is black
  // flashes on the plugin rendering surface.
  if (mozilla::ipc::MessageChannel::IsSpinLoopActive() && mPainting)
    return false;

  DeviceResetReason resetReason = DeviceResetReason::OK;
  if (gfxWindowsPlatform::GetPlatform()->DidRenderingDeviceReset(&resetReason)) {
    gfxCriticalNote << "(nsWindow) Detected device reset: " << (int)resetReason;

    gfxWindowsPlatform::GetPlatform()->UpdateRenderMode();

    nsTArray<nsWindow*> windows = EnumAllWindows();
    for (nsWindow* window : windows) {
      window->OnRenderingDeviceReset();
    }

    nsTArray<mozilla::dom::ContentParent*> children;
    mozilla::dom::ContentParent::GetAll(children);
    for (const auto& child : children) {
      child->OnCompositorDeviceReset();
    }

    gfxCriticalNote << "(nsWindow) Finished device reset.";
    return false;
  }

  // After we CallUpdateWindow to the child, occasionally a WM_PAINT message
  // is posted to the parent event loop with an empty update rect. Do a
  // dummy paint so that Windows stops dispatching WM_PAINT in an inifinite
  // loop. See bug 543788.
  if (IsPlugin()) {
    RECT updateRect;
    if (!GetUpdateRect(mWnd, &updateRect, FALSE) ||
        (updateRect.left == updateRect.right &&
         updateRect.top == updateRect.bottom)) {
      PAINTSTRUCT ps;
      BeginPaint(mWnd, &ps);
      EndPaint(mWnd, &ps);
      return true;
    }

    if (mWindowType == eWindowType_plugin_ipc_chrome) {
      // Fire off an async request to the plugin to paint its window
      mozilla::dom::ContentParent::SendAsyncUpdate(this);
      ValidateRect(mWnd, nullptr);
      return true;
    }

    PluginInstanceParent* instance = reinterpret_cast<PluginInstanceParent*>(
      ::GetPropW(mWnd, L"PluginInstanceParentProperty"));
    if (instance) {
      Unused << instance->CallUpdateWindow();
    } else {
      // We should never get here since in-process plugins should have
      // subclassed our HWND and handled WM_PAINT, but in some cases that
      // could fail. Return without asserting since it's not our fault.
      NS_WARNING("Plugin failed to subclass our window");
    }

    ValidateRect(mWnd, nullptr);
    return true;
  }

  if (GetLayerManager()->AsKnowsCompositor() && !mBounds.IsEqualEdges(mLastPaintBounds)) {
    // Do an early async composite so that we at least have something on the
    // screen in the right place, even if the content is out of date.
    GetLayerManager()->Composite();
  }
  mLastPaintBounds = mBounds;

  PAINTSTRUCT ps;

#ifdef MOZ_XUL
  if (!aDC && (eTransparencyTransparent == mTransparencyMode))
  {
    // For layered translucent windows all drawing should go to memory DC and no
    // WM_PAINT messages are normally generated. To support asynchronous painting
    // we force generation of WM_PAINT messages by invalidating window areas with
    // RedrawWindow, InvalidateRect or InvalidateRgn function calls.
    // BeginPaint/EndPaint must be called to make Windows think that invalid area
    // is painted. Otherwise it will continue sending the same message endlessly.
    ::BeginPaint(mWnd, &ps);
    ::EndPaint(mWnd, &ps);

    // We're guaranteed to have a widget proxy since we called GetLayerManager().
    aDC = mCompositorWidgetDelegate->GetTransparentDC();
  }
#endif

  mPainting = true;

#ifdef WIDGET_DEBUG_OUTPUT
  HRGN debugPaintFlashRegion = nullptr;
  HDC debugPaintFlashDC = nullptr;

  if (debug_WantPaintFlashing())
  {
    debugPaintFlashRegion = ::CreateRectRgn(0, 0, 0, 0);
    ::GetUpdateRgn(mWnd, debugPaintFlashRegion, TRUE);
    debugPaintFlashDC = ::GetDC(mWnd);
  }
#endif // WIDGET_DEBUG_OUTPUT

  HDC hDC = aDC ? aDC : (::BeginPaint(mWnd, &ps));
  mPaintDC = hDC;

#ifdef MOZ_XUL
  bool forceRepaint = aDC || (eTransparencyTransparent == mTransparencyMode);
#else
  bool forceRepaint = nullptr != aDC;
#endif
  nsIntRegion region = GetRegionToPaint(forceRepaint, ps, hDC);

  if (GetLayerManager()->AsKnowsCompositor()) {
    // We need to paint to the screen even if nothing changed, since if we
    // don't have a compositing window manager, our pixels could be stale.
    GetLayerManager()->SetNeedsComposite(true);
    GetLayerManager()->SendInvalidRegion(region);
  }

  RefPtr<nsWindow> strongThis(this);

  nsIWidgetListener* listener = GetPaintListener();
  if (listener) {
    listener->WillPaintWindow(this);
  }
  // Re-get the listener since the will paint notification may have killed it.
  listener = GetPaintListener();
  if (!listener) {
    return false;
  }

  if (GetLayerManager()->AsKnowsCompositor() && GetLayerManager()->NeedsComposite()) {
    GetLayerManager()->Composite();
    GetLayerManager()->SetNeedsComposite(false);
  }

  bool result = true;
  if (!region.IsEmpty() && listener)
  {
    // Should probably pass in a real region here, using GetRandomRgn
    // http://msdn.microsoft.com/library/default.asp?url=/library/en-us/gdi/clipping_4q0e.asp

#ifdef WIDGET_DEBUG_OUTPUT
    debug_DumpPaintEvent(stdout,
                         this,
                         region,
                         "noname",
                         (int32_t) mWnd);
#endif // WIDGET_DEBUG_OUTPUT

    switch (GetLayerManager()->GetBackendType()) {
      case LayersBackend::LAYERS_BASIC:
        {
          RefPtr<gfxASurface> targetSurface;

#if defined(MOZ_XUL)
          // don't support transparency for non-GDI rendering, for now
          if (eTransparencyTransparent == mTransparencyMode) {
            targetSurface = mBasicLayersSurface->EnsureTransparentSurface();
          }
#endif

          RefPtr<gfxWindowsSurface> targetSurfaceWin;
          if (!targetSurface)
          {
            uint32_t flags = (mTransparencyMode == eTransparencyOpaque) ? 0 :
                gfxWindowsSurface::FLAG_IS_TRANSPARENT;
            targetSurfaceWin = new gfxWindowsSurface(hDC, flags);
            targetSurface = targetSurfaceWin;
          }

          if (!targetSurface) {
            NS_ERROR("Invalid RenderMode!");
            return false;
          }

          RECT paintRect;
          ::GetClientRect(mWnd, &paintRect);
          RefPtr<DrawTarget> dt =
            gfxPlatform::GetPlatform()->CreateDrawTargetForSurface(targetSurface,
                                                                   IntSize(paintRect.right - paintRect.left,
                                                                   paintRect.bottom - paintRect.top));
          if (!dt || !dt->IsValid()) {
            gfxWarning() << "nsWindow::OnPaint failed in CreateDrawTargetForSurface";
            return false;
          }

          // don't need to double buffer with anything but GDI
          BufferMode doubleBuffering = mozilla::layers::BufferMode::BUFFER_NONE;
#ifdef MOZ_XUL
          switch (mTransparencyMode) {
            case eTransparencyGlass:
            case eTransparencyBorderlessGlass:
            default:
              // If we're not doing translucency, then double buffer
              doubleBuffering = mozilla::layers::BufferMode::BUFFERED;
              break;
            case eTransparencyTransparent:
              // If we're rendering with translucency, we're going to be
              // rendering the whole window; make sure we clear it first
              dt->ClearRect(Rect(0.f, 0.f,
                                 dt->GetSize().width, dt->GetSize().height));
              break;
          }
#else
          doubleBuffering = mozilla::layers::BufferMode::BUFFERED;
#endif

          RefPtr<gfxContext> thebesContext = gfxContext::CreateOrNull(dt);
          MOZ_ASSERT(thebesContext); // already checked draw target above

          {
            AutoLayerManagerSetup
              setupLayerManager(this, thebesContext, doubleBuffering);
            result = listener->PaintWindow(
              this, LayoutDeviceIntRegion::FromUnknownRegion(region));
          }

#ifdef MOZ_XUL
          if (eTransparencyTransparent == mTransparencyMode) {
            // Data from offscreen drawing surface was copied to memory bitmap of transparent
            // bitmap. Now it can be read from memory bitmap to apply alpha channel and after
            // that displayed on the screen.
            mBasicLayersSurface->RedrawTransparentWindow();
          }
#endif
        }
        break;
      case LayersBackend::LAYERS_CLIENT:
        {
          result = listener->PaintWindow(
            this, LayoutDeviceIntRegion::FromUnknownRegion(region));
          if (!gfxEnv::DisableForcePresent() && gfxWindowsPlatform::GetPlatform()->DwmCompositionEnabled()) {
            nsCOMPtr<nsIRunnable> event =
              NewRunnableMethod(this, &nsWindow::ForcePresent);
            NS_DispatchToMainThread(event);
          }
        }
        break;
      case LayersBackend::LAYERS_WR:
      {
        GetLayerManager()->Composite();
        break;
      }
      default:
        NS_ERROR("Unknown layers backend used!");
        break;
    }
  }

  if (!aDC) {
    ::EndPaint(mWnd, &ps);
  }

  mPaintDC = nullptr;
  mLastPaintEndTime = TimeStamp::Now();

#if defined(WIDGET_DEBUG_OUTPUT)
  if (debug_WantPaintFlashing())
  {
    // Only flash paint events which have not ignored the paint message.
    // Those that ignore the paint message aren't painting anything so there
    // is only the overhead of the dispatching the paint event.
    if (result) {
      ::InvertRgn(debugPaintFlashDC, debugPaintFlashRegion);
      PR_Sleep(PR_MillisecondsToInterval(30));
      ::InvertRgn(debugPaintFlashDC, debugPaintFlashRegion);
      PR_Sleep(PR_MillisecondsToInterval(30));
    }
    ::ReleaseDC(mWnd, debugPaintFlashDC);
    ::DeleteObject(debugPaintFlashRegion);
  }
#endif // WIDGET_DEBUG_OUTPUT

  mPainting = false;

  // Re-get the listener since painting may have killed it.
  listener = GetPaintListener();
  if (listener)
    listener->DidPaintWindow();

  if (aNestingLevel == 0 && ::GetUpdateRect(mWnd, nullptr, false)) {
    OnPaint(aDC, 1);
  }

  return result;
}

IntSize nsWindowGfx::GetIconMetrics(IconSizeType aSizeType) {
  int32_t width = ::GetSystemMetrics(sIconMetrics[aSizeType].xMetric);
  int32_t height = ::GetSystemMetrics(sIconMetrics[aSizeType].yMetric);

  if (width == 0 || height == 0) {
    width = height = sIconMetrics[aSizeType].defaultSize;
  }

  return IntSize(width, height);
}

nsresult nsWindowGfx::CreateIcon(imgIContainer *aContainer,
                                  bool aIsCursor,
                                  uint32_t aHotspotX,
                                  uint32_t aHotspotY,
                                  IntSize aScaledSize,
                                  HICON *aIcon) {

  MOZ_ASSERT((aScaledSize.width > 0 && aScaledSize.height > 0) ||
             (aScaledSize.width == 0 && aScaledSize.height == 0));

  // Get the image data
  RefPtr<SourceSurface> surface =
    aContainer->GetFrame(imgIContainer::FRAME_CURRENT,
                         imgIContainer::FLAG_SYNC_DECODE);
  NS_ENSURE_TRUE(surface, NS_ERROR_NOT_AVAILABLE);

  IntSize frameSize = surface->GetSize();
  if (frameSize.IsEmpty()) {
    return NS_ERROR_FAILURE;
  }

  IntSize iconSize(aScaledSize.width, aScaledSize.height);
  if (iconSize == IntSize(0, 0)) { // use frame's intrinsic size
    iconSize = frameSize;
  }

  RefPtr<DataSourceSurface> dataSurface;
  bool mappedOK;
  DataSourceSurface::MappedSurface map;

  if (iconSize != frameSize) {
    // Scale the surface
    dataSurface = Factory::CreateDataSourceSurface(iconSize,
                                                   SurfaceFormat::B8G8R8A8);
    NS_ENSURE_TRUE(dataSurface, NS_ERROR_FAILURE);
    mappedOK = dataSurface->Map(DataSourceSurface::MapType::READ_WRITE, &map);
    NS_ENSURE_TRUE(mappedOK, NS_ERROR_FAILURE);

    RefPtr<DrawTarget> dt =
      Factory::CreateDrawTargetForData(BackendType::CAIRO,
                                       map.mData,
                                       dataSurface->GetSize(),
                                       map.mStride,
                                       SurfaceFormat::B8G8R8A8);
    if (!dt) {
      gfxWarning() << "nsWindowGfx::CreatesIcon failed in CreateDrawTargetForData";
      return NS_ERROR_OUT_OF_MEMORY;
    }
    dt->DrawSurface(surface,
                    Rect(0, 0, iconSize.width, iconSize.height),
                    Rect(0, 0, frameSize.width, frameSize.height),
                    DrawSurfaceOptions(),
                    DrawOptions(1.0f, CompositionOp::OP_SOURCE));
  } else if (surface->GetFormat() != SurfaceFormat::B8G8R8A8) {
    // Convert format to SurfaceFormat::B8G8R8A8
    dataSurface = gfxUtils::
      CopySurfaceToDataSourceSurfaceWithFormat(surface,
                                               SurfaceFormat::B8G8R8A8);
    NS_ENSURE_TRUE(dataSurface, NS_ERROR_FAILURE);
    mappedOK = dataSurface->Map(DataSourceSurface::MapType::READ, &map);
  } else {
    dataSurface = surface->GetDataSurface();
    NS_ENSURE_TRUE(dataSurface, NS_ERROR_FAILURE);
    mappedOK = dataSurface->Map(DataSourceSurface::MapType::READ, &map);
  }
  NS_ENSURE_TRUE(dataSurface && mappedOK, NS_ERROR_FAILURE);
  MOZ_ASSERT(dataSurface->GetFormat() == SurfaceFormat::B8G8R8A8);

  uint8_t* data = nullptr;
  UniquePtr<uint8_t[]> autoDeleteArray;
  if (map.mStride == BytesPerPixel(dataSurface->GetFormat()) * iconSize.width) {
    // Mapped data is already packed
    data = map.mData;
  } else {
    // We can't use map.mData since the pixels are not packed (as required by
    // CreateDIBitmap, which is called under the DataToBitmap call below).
    //
    // We must unmap before calling SurfaceToPackedBGRA because it needs access
    // to the pixel data.
    dataSurface->Unmap();
    map.mData = nullptr;

    autoDeleteArray = SurfaceToPackedBGRA(dataSurface);
    data = autoDeleteArray.get();
    NS_ENSURE_TRUE(data, NS_ERROR_FAILURE);
  }

  HBITMAP bmp = DataToBitmap(data, iconSize.width, -iconSize.height, 32);
  uint8_t* a1data = Data32BitTo1Bit(data, iconSize.width, iconSize.height);
  if (map.mData) {
    dataSurface->Unmap();
  }
  if (!a1data) {
    return NS_ERROR_FAILURE;
  }

  HBITMAP mbmp = DataToBitmap(a1data, iconSize.width, -iconSize.height, 1);
  PR_Free(a1data);

  ICONINFO info = {0};
  info.fIcon = !aIsCursor;
  info.xHotspot = aHotspotX;
  info.yHotspot = aHotspotY;
  info.hbmMask = mbmp;
  info.hbmColor = bmp;

  HCURSOR icon = ::CreateIconIndirect(&info);
  ::DeleteObject(mbmp);
  ::DeleteObject(bmp);
  if (!icon)
    return NS_ERROR_FAILURE;
  *aIcon = icon;
  return NS_OK;
}

// Adjust cursor image data
uint8_t* nsWindowGfx::Data32BitTo1Bit(uint8_t* aImageData,
                                      uint32_t aWidth, uint32_t aHeight)
{
  // We need (aWidth + 7) / 8 bytes plus zero-padding up to a multiple of
  // 4 bytes for each row (HBITMAP requirement). Bug 353553.
  uint32_t outBpr = ((aWidth + 31) / 8) & ~3;

  // Allocate and clear mask buffer
  uint8_t* outData = (uint8_t*)PR_Calloc(outBpr, aHeight);
  if (!outData)
    return nullptr;

  int32_t *imageRow = (int32_t*)aImageData;
  for (uint32_t curRow = 0; curRow < aHeight; curRow++) {
    uint8_t *outRow = outData + curRow * outBpr;
    uint8_t mask = 0x80;
    for (uint32_t curCol = 0; curCol < aWidth; curCol++) {
      // Use sign bit to test for transparency, as alpha byte is highest byte
      if (*imageRow++ < 0)
        *outRow |= mask;

      mask >>= 1;
      if (!mask) {
        outRow ++;
        mask = 0x80;
      }
    }
  }

  return outData;
}

/**
 * Convert the given image data to a HBITMAP. If the requested depth is
 * 32 bit, a bitmap with an alpha channel will be returned.
 *
 * @param aImageData The image data to convert. Must use the format accepted
 *                   by CreateDIBitmap.
 * @param aWidth     With of the bitmap, in pixels.
 * @param aHeight    Height of the image, in pixels.
 * @param aDepth     Image depth, in bits. Should be one of 1, 24 and 32.
 *
 * @return The HBITMAP representing the image. Caller should call
 *         DeleteObject when done with the bitmap.
 *         On failure, nullptr will be returned.
 */
HBITMAP nsWindowGfx::DataToBitmap(uint8_t* aImageData,
                                  uint32_t aWidth,
                                  uint32_t aHeight,
                                  uint32_t aDepth)
{
  HDC dc = ::GetDC(nullptr);

  if (aDepth == 32) {
    // Alpha channel. We need the new header.
    BITMAPV4HEADER head = { 0 };
    head.bV4Size = sizeof(head);
    head.bV4Width = aWidth;
    head.bV4Height = aHeight;
    head.bV4Planes = 1;
    head.bV4BitCount = aDepth;
    head.bV4V4Compression = BI_BITFIELDS;
    head.bV4SizeImage = 0; // Uncompressed
    head.bV4XPelsPerMeter = 0;
    head.bV4YPelsPerMeter = 0;
    head.bV4ClrUsed = 0;
    head.bV4ClrImportant = 0;

    head.bV4RedMask   = 0x00FF0000;
    head.bV4GreenMask = 0x0000FF00;
    head.bV4BlueMask  = 0x000000FF;
    head.bV4AlphaMask = 0xFF000000;

    HBITMAP bmp = ::CreateDIBitmap(dc,
                                   reinterpret_cast<CONST BITMAPINFOHEADER*>(&head),
                                   CBM_INIT,
                                   aImageData,
                                   reinterpret_cast<CONST BITMAPINFO*>(&head),
                                   DIB_RGB_COLORS);
    ::ReleaseDC(nullptr, dc);
    return bmp;
  }

  char reserved_space[sizeof(BITMAPINFOHEADER) + sizeof(RGBQUAD) * 2];
  BITMAPINFOHEADER& head = *(BITMAPINFOHEADER*)reserved_space;

  head.biSize = sizeof(BITMAPINFOHEADER);
  head.biWidth = aWidth;
  head.biHeight = aHeight;
  head.biPlanes = 1;
  head.biBitCount = (WORD)aDepth;
  head.biCompression = BI_RGB;
  head.biSizeImage = 0; // Uncompressed
  head.biXPelsPerMeter = 0;
  head.biYPelsPerMeter = 0;
  head.biClrUsed = 0;
  head.biClrImportant = 0;
  
  BITMAPINFO& bi = *(BITMAPINFO*)reserved_space;

  if (aDepth == 1) {
    RGBQUAD black = { 0, 0, 0, 0 };
    RGBQUAD white = { 255, 255, 255, 0 };

    bi.bmiColors[0] = white;
    bi.bmiColors[1] = black;
  }

  HBITMAP bmp = ::CreateDIBitmap(dc, &head, CBM_INIT, aImageData, &bi, DIB_RGB_COLORS);
  ::ReleaseDC(nullptr, dc);
  return bmp;
}
