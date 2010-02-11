/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*
 * nsWindowGfx - Painting and aceleration.
 */

// XXX Future: this should really be a stand alone class stored as
// a member of nsWindow with getters and setters for things like render
// mode and methods for handling paint.

/**************************************************************
 **************************************************************
 **
 ** BLOCK: Includes
 **
 ** Include headers.
 **
 **************************************************************
 **************************************************************/

#ifdef MOZ_IPC
#include "mozilla/plugins/PluginInstanceParent.h"
using mozilla::plugins::PluginInstanceParent;
#endif

#include "nsWindowGfx.h"
#include <windows.h>
#include "nsIRegion.h"
#include "gfxImageSurface.h"
#include "gfxWindowsSurface.h"
#include "gfxWindowsPlatform.h"
#include "nsGfxCIID.h"
#include "gfxContext.h"
#include "nsIRenderingContext.h"
#include "nsIDeviceContext.h"
#include "prmem.h"

#ifndef WINCE
#include "nsUXThemeData.h"
#include "nsUXThemeConstants.h"
#endif

extern "C" {
#include "pixman.h"
}

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

static nsAutoPtr<PRUint8>  sSharedSurfaceData;
static gfxIntSize          sSharedSurfaceSize;

/**************************************************************
 *
 * SECTION: global variables.
 *
 **************************************************************/

#ifdef CAIRO_HAS_DDRAW_SURFACE
// XXX Still need to handle clean-up!!
static LPDIRECTDRAW glpDD                         = NULL;
static LPDIRECTDRAWSURFACE glpDDPrimary           = NULL;
static LPDIRECTDRAWCLIPPER glpDDClipper           = NULL;
static LPDIRECTDRAWSURFACE glpDDSecondary         = NULL;
static nsAutoPtr<gfxDDrawSurface> gpDDSurf        = NULL;
static DDSURFACEDESC gDDSDSecondary;
#endif

static NS_DEFINE_CID(kRegionCID,                  NS_REGION_CID);
static NS_DEFINE_IID(kRenderingContextCID,        NS_RENDERING_CONTEXT_CID);

/**************************************************************
 **************************************************************
 **
 ** BLOCK: nsWindowGfx impl.
 **
 ** Misc. graphics related utilities.
 **
 **************************************************************
 **************************************************************/

static PRBool
IsRenderMode(gfxWindowsPlatform::RenderMode rmode)
{
  return gfxWindowsPlatform::GetPlatform()->GetRenderMode() == rmode;
}

void
nsWindowGfx::AddRECTToRegion(const RECT& aRect, nsIRegion* aRegion)
{
  aRegion->Union(aRect.left, aRect.top, aRect.right - aRect.left, aRect.bottom - aRect.top);
}

already_AddRefed<nsIRegion>
nsWindowGfx::ConvertHRGNToRegion(HRGN aRgn)
{
  NS_ASSERTION(aRgn, "Don't pass NULL region here");

  nsCOMPtr<nsIRegion> region = do_CreateInstance(kRegionCID);
  if (!region)
    return nsnull;

  region->Init();

  DWORD size = ::GetRegionData(aRgn, 0, NULL);
  nsAutoTArray<PRUint8,100> buffer;
  if (!buffer.SetLength(size))
    return region.forget();

  RGNDATA* data = reinterpret_cast<RGNDATA*>(buffer.Elements());
  if (!::GetRegionData(aRgn, size, data))
    return region.forget();

  if (data->rdh.nCount > MAX_RECTS_IN_REGION) {
    AddRECTToRegion(data->rdh.rcBound, region);
    return region.forget();
  }

  RECT* rects = reinterpret_cast<RECT*>(data->Buffer);
  for (PRUint32 i = 0; i < data->rdh.nCount; ++i) {
    RECT* r = rects + i;
    AddRECTToRegion(*r, region);
  }

  return region.forget();
}

#ifdef CAIRO_HAS_DDRAW_SURFACE
PRBool
nsWindowGfx::InitDDraw()
{
  HRESULT hr;

  hr = DirectDrawCreate(NULL, &glpDD, NULL);
  NS_ENSURE_SUCCESS(hr, PR_FALSE);

  hr = glpDD->SetCooperativeLevel(NULL, DDSCL_NORMAL);
  NS_ENSURE_SUCCESS(hr, PR_FALSE);

  DDSURFACEDESC ddsd;
  memset(&ddsd, 0, sizeof(ddsd));
  ddsd.dwSize = sizeof(ddsd);
  ddsd.dwFlags = DDSD_CAPS;
  ddsd.ddpfPixelFormat.dwSize = sizeof(ddsd.ddpfPixelFormat);
  ddsd.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE;

  hr = glpDD->CreateSurface(&ddsd, &glpDDPrimary, NULL);
  NS_ENSURE_SUCCESS(hr, PR_FALSE);

  hr = glpDD->CreateClipper(0, &glpDDClipper, NULL);
  NS_ENSURE_SUCCESS(hr, PR_FALSE);

  hr = glpDDPrimary->SetClipper(glpDDClipper);
  NS_ENSURE_SUCCESS(hr, PR_FALSE);

  // We do not use the cairo ddraw surface for IMAGE_DDRAW16.  Instead, we
  // use an 24bpp image surface, convert that to 565, then blit using ddraw.
  if (!IsRenderMode(gfxWindowsPlatform::RENDER_IMAGE_DDRAW16)) {
    gfxIntSize screen_size(GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN));
    gpDDSurf = new gfxDDrawSurface(glpDD, screen_size, gfxASurface::ImageFormatRGB24);
    if (!gpDDSurf) {
      /*XXX*/
      fprintf(stderr, "couldn't create ddsurf\n");
      return PR_FALSE;
    }
  }

  return PR_TRUE;
}
#endif

/**************************************************************
 **************************************************************
 **
 ** BLOCK: nsWindow impl.
 **
 ** Paint related nsWindow methods.
 **
 **************************************************************
 **************************************************************/

void nsWindowGfx::OnSettingsChangeGfx(WPARAM wParam)
{
#if defined(WINCE_WINDOWS_MOBILE)
  if (wParam == SETTINGCHANGE_RESET) {
    if (glpDDSecondary) {
      glpDDSecondary->Release();
      glpDDSecondary = NULL;
    }

    if(glpDD)
      glpDD->RestoreAllSurfaces();
  }
#endif
}

void nsWindow::SetUpForPaint(HDC aHDC)
{
  ::SetBkColor (aHDC, NSRGB_2_COLOREF(mBackground));
  ::SetTextColor(aHDC, NSRGB_2_COLOREF(mForeground));
  ::SetBkMode (aHDC, TRANSPARENT);
}

// GetRegionToPaint returns the invalidated region that needs to be painted
// it's abstracted out because Windows XP/Vista/7 handles this for us, but
// we need to keep track of it our selves for Windows CE and Windows Mobile

nsCOMPtr<nsIRegion> nsWindow::GetRegionToPaint(PRBool aForceFullRepaint,
                                               PAINTSTRUCT ps, HDC aDC)
{ 
  HRGN paintRgn = NULL;
  nsCOMPtr<nsIRegion> paintRgnWin;
  if (aForceFullRepaint) {
    RECT paintRect;
    ::GetClientRect(mWnd, &paintRect);
    paintRgn = ::CreateRectRgn(paintRect.left, paintRect.top, 
                               paintRect.right, paintRect.bottom);
    if (paintRgn) {
      paintRgnWin = nsWindowGfx::ConvertHRGNToRegion(paintRgn);
      ::DeleteObject(paintRgn);
      return paintRgnWin;
    }
  }
#ifndef WINCE
  paintRgn = ::CreateRectRgn(0, 0, 0, 0);
  if (paintRgn != NULL) {
    int result = GetRandomRgn(aDC, paintRgn, SYSRGN);
    if (result == 1) {
      POINT pt = {0,0};
      ::MapWindowPoints(NULL, mWnd, &pt, 1);
      ::OffsetRgn(paintRgn, pt.x, pt.y);
    }
    paintRgnWin = nsWindowGfx::ConvertHRGNToRegion(paintRgn);
    ::DeleteObject(paintRgn);
  }
#else
# ifdef WINCE_WINDOWS_MOBILE
  paintRgn = ::CreateRectRgn(0, 0, 0, 0);
  if (paintRgn != NULL) {
    int result = GetUpdateRgn(mWnd, paintRgn, FALSE);
    if (result == 1) {
      POINT pt = {0,0};
      ::MapWindowPoints(NULL, mWnd, &pt, 1);
      ::OffsetRgn(paintRgn, pt.x, pt.y);
    }
    paintRgnWin = nsWindowGfx::ConvertHRGNToRegion(paintRgn);
    ::DeleteObject(paintRgn);
  }
# endif
  paintRgn = ::CreateRectRgn(ps.rcPaint.left, ps.rcPaint.top,
                             ps.rcPaint.right, ps.rcPaint.bottom);
  if (paintRgn) {
    paintRgnWin = nsWindowGfx::ConvertHRGNToRegion(paintRgn);
    ::DeleteObject(paintRgn);
  }
#endif
  return paintRgnWin;
}

#define WORDSSIZE(x) ((x).width * (x).height)
static PRBool
EnsureSharedSurfaceSize(gfxIntSize size)
{
  gfxIntSize screenSize;
  screenSize.height = GetSystemMetrics(SM_CYSCREEN);
  screenSize.width = GetSystemMetrics(SM_CXSCREEN);

  if (WORDSSIZE(screenSize) > WORDSSIZE(size))
    size = screenSize;

  if (WORDSSIZE(screenSize) < WORDSSIZE(size))
    NS_WARNING("Trying to create a shared surface larger than the screen");

  if (!sSharedSurfaceData || (WORDSSIZE(size) > WORDSSIZE(sSharedSurfaceSize))) {
    sSharedSurfaceSize = size;
    sSharedSurfaceData = nsnull;
    sSharedSurfaceData = (PRUint8 *)malloc(WORDSSIZE(sSharedSurfaceSize) * 4);
  }

  return (sSharedSurfaceData != nsnull);
}

PRBool nsWindow::OnPaint(HDC aDC)
{
#ifdef MOZ_IPC
  if (mWindowType == eWindowType_plugin) {

    /**
     * After we CallUpdateWindow to the child, occasionally a WM_PAINT message
     * is posted to the parent event loop with an empty update rect. Ignore
     * this paint message, since dispatching it again may cause an infinite
     * loop. See bug 543788.
     */
    RECT updateRect;
    if (!GetUpdateRect(mWnd, &updateRect, FALSE) ||
        (updateRect.left == updateRect.right &&
         updateRect.top == updateRect.bottom))
      return PR_TRUE;

    PluginInstanceParent* instance = reinterpret_cast<PluginInstanceParent*>(
      ::GetPropW(mWnd, L"PluginInstanceParentProperty"));
    if (instance) {
      instance->CallUpdateWindow();
      ValidateRect(mWnd, NULL);
      return PR_TRUE;
    }
  }
#endif

  nsPaintEvent willPaintEvent(PR_TRUE, NS_WILL_PAINT, this);
  DispatchWindowEvent(&willPaintEvent);

#ifdef CAIRO_HAS_DDRAW_SURFACE
  if (IsRenderMode(gfxWindowsPlatform::RENDER_IMAGE_DDRAW16)) {
    return OnPaintImageDDraw16();
  }
#endif

  PRBool result = PR_TRUE;
  PAINTSTRUCT ps;
  nsEventStatus eventStatus = nsEventStatus_eIgnore;

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

    aDC = mMemoryDC;
  }
#endif

  mPainting = PR_TRUE;

#ifdef WIDGET_DEBUG_OUTPUT
  HRGN debugPaintFlashRegion = NULL;
  HDC debugPaintFlashDC = NULL;

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
  PRBool forceRepaint = aDC || (eTransparencyTransparent == mTransparencyMode);
#else
  PRBool forceRepaint = NULL != aDC;
#endif
  nsCOMPtr<nsIRegion> paintRgnWin = GetRegionToPaint(forceRepaint, ps, hDC);

  if (paintRgnWin &&
      !paintRgnWin->IsEmpty() &&
      mEventCallback)
  {
    // generate the event and call the event callback
    nsPaintEvent event(PR_TRUE, NS_PAINT, this);

    InitEvent(event);

    event.region = paintRgnWin;
    event.rect = nsnull;
 
    // Should probably pass in a real region here, using GetRandomRgn
    // http://msdn.microsoft.com/library/default.asp?url=/library/en-us/gdi/clipping_4q0e.asp

#ifdef WIDGET_DEBUG_OUTPUT
    debug_DumpPaintEvent(stdout,
                         this,
                         &event,
                         nsCAutoString("noname"),
                         (PRInt32) mWnd);
#endif // WIDGET_DEBUG_OUTPUT

    nsRefPtr<gfxASurface> targetSurface;

#if defined(MOZ_XUL)
    // don't support transparency for non-GDI rendering, for now
    if (IsRenderMode(gfxWindowsPlatform::RENDER_GDI) && eTransparencyTransparent == mTransparencyMode) {
      if (mTransparentSurface == nsnull)
        SetupTranslucentWindowMemoryBitmap(mTransparencyMode);
      targetSurface = mTransparentSurface;
    }
#endif

    nsRefPtr<gfxWindowsSurface> targetSurfaceWin;
    if (!targetSurface &&
        IsRenderMode(gfxWindowsPlatform::RENDER_GDI))
    {
      targetSurfaceWin = new gfxWindowsSurface(hDC);
      targetSurface = targetSurfaceWin;
    }

#ifdef CAIRO_HAS_DDRAW_SURFACE
    nsRefPtr<gfxDDrawSurface> targetSurfaceDDraw;
    if (!targetSurface &&
        (IsRenderMode(gfxWindowsPlatform::RENDER_DDRAW) ||
         IsRenderMode(gfxWindowsPlatform::RENDER_DDRAW_GL)))
    {
      if (!glpDD) {
        if (!nsWindowGfx::InitDDraw()) {
          NS_WARNING("DirectDraw init failed; falling back to RENDER_IMAGE_STRETCH24");
          gfxWindowsPlatform::GetPlatform()->SetRenderMode(gfxWindowsPlatform::RENDER_IMAGE_STRETCH24);
          goto DDRAW_FAILED;
        }
      }

      // create a rect that maps the window in screen space
      // create a new sub-surface that aliases this one
      RECT winrect;
      GetClientRect(mWnd, &winrect);
      MapWindowPoints(mWnd, NULL, (LPPOINT)&winrect, 2);

      targetSurfaceDDraw = new gfxDDrawSurface(gpDDSurf.get(), winrect);
      targetSurface = targetSurfaceDDraw;
    }
#endif

DDRAW_FAILED:
    nsRefPtr<gfxImageSurface> targetSurfaceImage;
    if (!targetSurface &&
        (IsRenderMode(gfxWindowsPlatform::RENDER_IMAGE_STRETCH32) ||
         IsRenderMode(gfxWindowsPlatform::RENDER_IMAGE_STRETCH24)))
    {
      gfxIntSize surfaceSize(ps.rcPaint.right - ps.rcPaint.left,
                             ps.rcPaint.bottom - ps.rcPaint.top);

      if (!EnsureSharedSurfaceSize(surfaceSize)) {
        NS_ERROR("Couldn't allocate a shared image surface!");
        return NS_ERROR_FAILURE;
      }

      // don't use the shared surface directly; instead, create a new one
      // that just reuses its buffer.
      targetSurfaceImage = new gfxImageSurface(sSharedSurfaceData.get(),
                                               surfaceSize,
                                               surfaceSize.width * 4,
                                               gfxASurface::ImageFormatRGB24);

      if (targetSurfaceImage && !targetSurfaceImage->CairoStatus()) {
        targetSurfaceImage->SetDeviceOffset(gfxPoint(-ps.rcPaint.left, -ps.rcPaint.top));
        targetSurface = targetSurfaceImage;
      }
    }

    if (!targetSurface) {
      NS_ERROR("Invalid RenderMode!");
      return NS_ERROR_FAILURE;
    }

    nsRefPtr<gfxContext> thebesContext = new gfxContext(targetSurface);
    thebesContext->SetFlag(gfxContext::FLAG_DESTINED_FOR_SCREEN);

#ifdef WINCE
    thebesContext->SetFlag(gfxContext::FLAG_SIMPLIFY_OPERATORS);
#endif

    // don't need to double buffer with anything but GDI
    if (IsRenderMode(gfxWindowsPlatform::RENDER_GDI)) {
# if defined(MOZ_XUL) && !defined(WINCE)
      if (eTransparencyGlass == mTransparencyMode && nsUXThemeData::sHaveCompositor) {
        thebesContext->PushGroup(gfxASurface::CONTENT_COLOR_ALPHA);
      } else if (eTransparencyTransparent == mTransparencyMode) {
        // If we're rendering with translucency, we're going to be
        // rendering the whole window; make sure we clear it first
        thebesContext->SetOperator(gfxContext::OPERATOR_CLEAR);
        thebesContext->Paint();
        thebesContext->SetOperator(gfxContext::OPERATOR_OVER);
      } else
#endif
      {
        // If we're not doing translucency, then double buffer
        thebesContext->PushGroup(gfxASurface::CONTENT_COLOR);
      }
    }

    nsCOMPtr<nsIRenderingContext> rc;
    nsresult rv = mContext->CreateRenderingContextInstance (*getter_AddRefs(rc));
    if (NS_FAILED(rv)) {
      NS_WARNING("CreateRenderingContextInstance failed");
      return PR_FALSE;
    }

    rv = rc->Init(mContext, thebesContext);
    if (NS_FAILED(rv)) {
      NS_WARNING("RC::Init failed");
      return PR_FALSE;
    }

    event.renderingContext = rc;
    result = DispatchWindowEvent(&event, eventStatus);
    event.renderingContext = nsnull;

#ifdef MOZ_XUL
    if (IsRenderMode(gfxWindowsPlatform::RENDER_GDI) &&
        eTransparencyTransparent == mTransparencyMode) {
      // Data from offscreen drawing surface was copied to memory bitmap of transparent
      // bitmap. Now it can be read from memory bitmap to apply alpha channel and after
      // that displayed on the screen.
      UpdateTranslucentWindow();
    } else
#endif
    if (result) {
      if (IsRenderMode(gfxWindowsPlatform::RENDER_GDI)) {
        // Only update if DispatchWindowEvent returned TRUE; otherwise, nothing handled
        // this, and we'll just end up painting with black.
        thebesContext->PopGroupToSource();
        thebesContext->SetOperator(gfxContext::OPERATOR_SOURCE);
        thebesContext->Paint();
      } else if (IsRenderMode(gfxWindowsPlatform::RENDER_DDRAW) ||
                 IsRenderMode(gfxWindowsPlatform::RENDER_DDRAW_GL))
      {
#ifdef CAIRO_HAS_DDRAW_SURFACE
        // blit with direct draw
        HRESULT hr = glpDDClipper->SetHWnd(0, mWnd);

#ifdef DEBUG
        if (FAILED(hr))
          DDError("SetHWnd", hr);
#endif

        // blt from the affected area from the window back-buffer to the
        // screen-relative coordinates of the window paint area
        RECT dst_rect = ps.rcPaint;
        MapWindowPoints(mWnd, NULL, (LPPOINT)&dst_rect, 2);
        hr = glpDDPrimary->Blt(&dst_rect,
                               gpDDSurf->GetDDSurface(),
                               &dst_rect,
                               DDBLT_WAITNOTBUSY,
                               NULL);
#ifdef DEBUG
        if (FAILED(hr))
          DDError("SetHWnd", hr);
#endif
#endif
      } else if (IsRenderMode(gfxWindowsPlatform::RENDER_IMAGE_STRETCH24) ||
                 IsRenderMode(gfxWindowsPlatform::RENDER_IMAGE_STRETCH32)) 
      {
        gfxIntSize surfaceSize = targetSurfaceImage->GetSize();

        // Just blit this directly
        BITMAPINFOHEADER bi;
        memset(&bi, 0, sizeof(BITMAPINFOHEADER));
        bi.biSize = sizeof(BITMAPINFOHEADER);
        bi.biWidth = surfaceSize.width;
        bi.biHeight = - surfaceSize.height;
        bi.biPlanes = 1;
        bi.biBitCount = 32;
        bi.biCompression = BI_RGB;

        if (IsRenderMode(gfxWindowsPlatform::RENDER_IMAGE_STRETCH24)) {
          // On Windows CE/Windows Mobile, 24bpp packed-pixel sources
          // seem to be far faster to blit than 32bpp (see bug 484864).
          // So, convert the bits to 24bpp by stripping out the unused
          // alpha byte.  24bpp DIBs also have scanlines that are 4-byte
          // aligned though, so that must be taken into account.
          int srcstride = surfaceSize.width*4;
          int dststride = surfaceSize.width*3;
          dststride = (dststride + 3) & ~3;

          // Convert in place
          for (int j = 0; j < surfaceSize.height; ++j) {
            unsigned int *src = (unsigned int*) (targetSurfaceImage->Data() + j*srcstride);
            unsigned int *dst = (unsigned int*) (targetSurfaceImage->Data() + j*dststride);

            // go 4 pixels at a time, since each 4 pixels
            // turns into 3 DWORDs when converted into BGR:
            // BGRx BGRx BGRx BGRx -> BGRB GRBG RBGR
            //
            // However, since we're dealing with little-endian ints, this is actually:
            // xRGB xrgb xRGB xrgb -> bRGB GBrg rgbR
            int width_left = surfaceSize.width;
            while (width_left >= 4) {
              unsigned int a = *src++;
              unsigned int b = *src++;
              unsigned int c = *src++;
              unsigned int d = *src++;

              *dst++ =  (a & 0x00ffffff)        | (b << 24);
              *dst++ = ((b & 0x00ffff00) >> 8)  | (c << 16);
              *dst++ = ((c & 0x00ff0000) >> 16) | (d << 8);

              width_left -= 4;
            }

            // then finish up whatever number of pixels are left,
            // using bytes.
            unsigned char *bsrc = (unsigned char*) src;
            unsigned char *bdst = (unsigned char*) dst;
            switch (width_left) {
              case 3:
                *bdst++ = *bsrc++;
                *bdst++ = *bsrc++;
                *bdst++ = *bsrc++;
                bsrc++;
              case 2:
                *bdst++ = *bsrc++;
                *bdst++ = *bsrc++;
                *bdst++ = *bsrc++;
                bsrc++;
              case 1:
                *bdst++ = *bsrc++;
                *bdst++ = *bsrc++;
                *bdst++ = *bsrc++;
                bsrc++;
              case 0:
                break;
            }
          }

          bi.biBitCount = 24;
        }

        StretchDIBits(hDC,
                      ps.rcPaint.left, ps.rcPaint.top,
                      surfaceSize.width, surfaceSize.height,
                      0, 0,
                      surfaceSize.width, surfaceSize.height,
                      targetSurfaceImage->Data(),
                      (BITMAPINFO*) &bi,
                      DIB_RGB_COLORS,
                      SRCCOPY);
      }
    }
  }

  if (!aDC) {
    ::EndPaint(mWnd, &ps);
  }

  mPaintDC = nsnull;

#if defined(WIDGET_DEBUG_OUTPUT) && !defined(WINCE)
  if (debug_WantPaintFlashing())
  {
    // Only flash paint events which have not ignored the paint message.
    // Those that ignore the paint message aren't painting anything so there
    // is only the overhead of the dispatching the paint event.
    if (nsEventStatus_eIgnore != eventStatus) {
      ::InvertRgn(debugPaintFlashDC, debugPaintFlashRegion);
      PR_Sleep(PR_MillisecondsToInterval(30));
      ::InvertRgn(debugPaintFlashDC, debugPaintFlashRegion);
      PR_Sleep(PR_MillisecondsToInterval(30));
    }
    ::ReleaseDC(mWnd, debugPaintFlashDC);
    ::DeleteObject(debugPaintFlashRegion);
  }
#endif // WIDGET_DEBUG_OUTPUT && !WINCE

  mPainting = PR_FALSE;

  return result;
}

nsresult nsWindowGfx::CreateIcon(imgIContainer *aContainer,
                                  PRBool aIsCursor,
                                  PRUint32 aHotspotX,
                                  PRUint32 aHotspotY,
                                  HICON *aIcon) {

  nsresult rv;
  PRInt32 maxWidth = GetSystemMetrics(SM_CXICON);
  PRInt32 maxHeight = GetSystemMetrics(SM_CYICON);

  if (!maxWidth || !maxHeight)
    return NS_ERROR_UNEXPECTED;

  PRUint32 nFrames;
  rv = aContainer->GetNumFrames(&nFrames);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!nFrames)
    return NS_ERROR_INVALID_ARG;

  // Get the image data
  nsRefPtr<gfxImageSurface> frame;
  aContainer->CopyFrame(imgIContainer::FRAME_CURRENT,
                        imgIContainer::FLAG_SYNC_DECODE,
                        getter_AddRefs(frame));
  if (!frame)
    return NS_ERROR_NOT_AVAILABLE;

  PRUint8 *data = frame->Data();

  PRInt32 width = frame->Width();
  PRInt32 height = frame->Height();

  if (width > maxWidth || height > maxHeight)
    return NS_ERROR_INVALID_ARG;

  HBITMAP bmp = DataToBitmap(data, width, -height, 32);
  PRUint8* a1data = Data32BitTo1Bit(data, width, height);
  if (!a1data) {
    return NS_ERROR_FAILURE;
  }

  HBITMAP mbmp = DataToBitmap(a1data, width, -height, 1);
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
PRUint8* nsWindowGfx::Data32BitTo1Bit(PRUint8* aImageData,
                                      PRUint32 aWidth, PRUint32 aHeight)
{
  // We need (aWidth + 7) / 8 bytes plus zero-padding up to a multiple of
  // 4 bytes for each row (HBITMAP requirement). Bug 353553.
  PRUint32 outBpr = ((aWidth + 31) / 8) & ~3;

  // Allocate and clear mask buffer
  PRUint8* outData = (PRUint8*)PR_Calloc(outBpr, aHeight);
  if (!outData)
    return NULL;

  PRInt32 *imageRow = (PRInt32*)aImageData;
  for (PRUint32 curRow = 0; curRow < aHeight; curRow++) {
    PRUint8 *outRow = outData + curRow * outBpr;
    PRUint8 mask = 0x80;
    for (PRUint32 curCol = 0; curCol < aWidth; curCol++) {
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

PRBool nsWindowGfx::IsCursorTranslucencySupported()
{
#ifdef WINCE
  return PR_FALSE;
#else
  static PRBool didCheck = PR_FALSE;
  static PRBool isSupported = PR_FALSE;
  if (!didCheck) {
    didCheck = PR_TRUE;
    // Cursor translucency is supported on Windows XP and newer
    isSupported = nsWindow::GetWindowsVersion() >= 0x501;
  }

  return isSupported;
#endif
}

/**
 * Convert the given image data to a HBITMAP. If the requested depth is
 * 32 bit and the OS supports translucency, a bitmap with an alpha channel
 * will be returned.
 *
 * @param aImageData The image data to convert. Must use the format accepted
 *                   by CreateDIBitmap.
 * @param aWidth     With of the bitmap, in pixels.
 * @param aHeight    Height of the image, in pixels.
 * @param aDepth     Image depth, in bits. Should be one of 1, 24 and 32.
 *
 * @return The HBITMAP representing the image. Caller should call
 *         DeleteObject when done with the bitmap.
 *         On failure, NULL will be returned.
 */
HBITMAP nsWindowGfx::DataToBitmap(PRUint8* aImageData,
                                  PRUint32 aWidth,
                                  PRUint32 aHeight,
                                  PRUint32 aDepth)
{
#ifndef WINCE
  HDC dc = ::GetDC(NULL);

  if (aDepth == 32 && IsCursorTranslucencySupported()) {
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
    ::ReleaseDC(NULL, dc);
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
  ::ReleaseDC(NULL, dc);
  return bmp;
#else
  return nsnull;
#endif
}


// Windows Mobile Special image/direct draw painting fun
#if defined(CAIRO_HAS_DDRAW_SURFACE)
PRBool nsWindow::OnPaintImageDDraw16()
{
  PRBool result = PR_FALSE;
  PAINTSTRUCT ps;
  nsPaintEvent event(PR_TRUE, NS_PAINT, this);
  gfxIntSize surfaceSize;
  nsRefPtr<gfxImageSurface> targetSurfaceImage;
  nsRefPtr<gfxContext> thebesContext;
  nsCOMPtr<nsIRenderingContext> rc;
  nsEventStatus eventStatus = nsEventStatus_eIgnore;
  PRInt32 brx, bry, brw, brh;
  gfxIntSize newSize;
  newSize.height = GetSystemMetrics(SM_CYSCREEN);
  newSize.width = GetSystemMetrics(SM_CXSCREEN);
  mPainting = PR_TRUE;

  HDC hDC = ::BeginPaint(mWnd, &ps);
  mPaintDC = hDC;
  nsCOMPtr<nsIRegion> paintRgnWin = GetRegionToPaint(PR_FALSE, ps, hDC);

  if (!paintRgnWin || paintRgnWin->IsEmpty() || !mEventCallback) {
    result = PR_TRUE;
    goto cleanup;
  }

  InitEvent(event);
  
  event.region = paintRgnWin;
  event.rect = nsnull;
  
  if (!glpDD) {
    if (!nsWindowGfx::InitDDraw()) {
      NS_WARNING("DirectDraw init failed.  Giving up.");
      goto cleanup;
    }
  }

  if (!glpDDSecondary) {

    memset(&gDDSDSecondary, 0, sizeof (gDDSDSecondary));
    memset(&gDDSDSecondary.ddpfPixelFormat, 0, sizeof(gDDSDSecondary.ddpfPixelFormat));
    
    gDDSDSecondary.dwSize = sizeof (gDDSDSecondary);
    gDDSDSecondary.ddpfPixelFormat.dwSize = sizeof(gDDSDSecondary.ddpfPixelFormat);
    
    gDDSDSecondary.dwFlags = DDSD_HEIGHT | DDSD_WIDTH | DDSD_PIXELFORMAT;

    gDDSDSecondary.dwHeight = newSize.height;
    gDDSDSecondary.dwWidth  = newSize.width;

    gDDSDSecondary.ddpfPixelFormat.dwFlags = DDPF_RGB;
    gDDSDSecondary.ddpfPixelFormat.dwRGBBitCount = 16;
    gDDSDSecondary.ddpfPixelFormat.dwRBitMask = 0xf800;
    gDDSDSecondary.ddpfPixelFormat.dwGBitMask = 0x07e0;
    gDDSDSecondary.ddpfPixelFormat.dwBBitMask = 0x001f;
    
    HRESULT hr = glpDD->CreateSurface(&gDDSDSecondary, &glpDDSecondary, 0);
    if (FAILED(hr)) {
#ifdef DEBUG
      DDError("CreateSurface renderer", hr);
#endif
      goto cleanup;
    }
  }

  paintRgnWin->GetBoundingBox(&brx, &bry, &brw, &brh);
  surfaceSize = gfxIntSize(brw, brh);
  
  if (!EnsureSharedSurfaceSize(surfaceSize))
    goto cleanup;

  targetSurfaceImage = new gfxImageSurface(sSharedSurfaceData.get(),
                                           surfaceSize,
                                           surfaceSize.width * 4,
                                           gfxASurface::ImageFormatRGB24);
    
  if (!targetSurfaceImage || targetSurfaceImage->CairoStatus())
    goto cleanup;
    
  targetSurfaceImage->SetDeviceOffset(gfxPoint(-brx, -bry));
  
  thebesContext = new gfxContext(targetSurfaceImage);
  thebesContext->SetFlag(gfxContext::FLAG_DESTINED_FOR_SCREEN);
  thebesContext->SetFlag(gfxContext::FLAG_SIMPLIFY_OPERATORS);
    
  nsresult rv = mContext->CreateRenderingContextInstance (*getter_AddRefs(rc));
  if (NS_FAILED(rv))
    goto cleanup;
  
  rv = rc->Init(mContext, thebesContext);
  if (NS_FAILED(rv))
    goto cleanup;
    
  event.renderingContext = rc;
  PRBool res = DispatchWindowEvent(&event, eventStatus);
  event.renderingContext = nsnull;
  
  if (!res && eventStatus  == nsEventStatus_eConsumeNoDefault)
    goto cleanup;

  nsRegionRectSet *rects = nsnull;
  RECT r;
  paintRgnWin->GetRects(&rects);
  
  HRESULT hr = glpDDSecondary->Lock(0, &gDDSDSecondary, DDLOCK_WAITNOTBUSY | DDLOCK_DISCARD, 0); 
  if (FAILED(hr))
    goto cleanup;

  pixman_image_t *srcPixmanImage = 
    pixman_image_create_bits(PIXMAN_x8r8g8b8, surfaceSize.width,
                             surfaceSize.height, 
                             (uint32_t*) sSharedSurfaceData.get(),
                             surfaceSize.width * 4);
  
  pixman_image_t *dstPixmanImage = 
    pixman_image_create_bits(PIXMAN_r5g6b5, gDDSDSecondary.dwWidth,
                             gDDSDSecondary.dwHeight,
                             (uint32_t*) gDDSDSecondary.lpSurface,
                             gDDSDSecondary.dwWidth * 2);
  

  for (unsigned int i = 0; i < rects->mNumRects; i++) {
    pixman_image_composite(PIXMAN_OP_SRC, srcPixmanImage, NULL, dstPixmanImage,
                           rects->mRects[i].x - brx, rects->mRects[i].y - bry, 
                           0, 0, 
                           rects->mRects[i].x, rects->mRects[i].y, 
                           rects->mRects[i].width, rects->mRects[i].height);
    
  } 
  
  pixman_image_unref(dstPixmanImage);
  pixman_image_unref(srcPixmanImage);

  hr = glpDDSecondary->Unlock(0);
  if (FAILED(hr))
    goto cleanup;
  
  hr = glpDDClipper->SetHWnd(0, mWnd);
  if (FAILED(hr))
    goto cleanup;
  
  for (unsigned int i = 0; i < rects->mNumRects; i++) {  
    r.left = rects->mRects[i].x;
    r.top = rects->mRects[i].y;
    r.right = rects->mRects[i].width + rects->mRects[i].x;
    r.bottom = rects->mRects[i].height + rects->mRects[i].y;
    RECT renderRect = r;
    SetLastError(0); // See http://msdn.microsoft.com/en-us/library/dd145046%28VS.85%29.aspx
    MapWindowPoints(mWnd, 0, (LPPOINT)&renderRect, 2);
    hr = glpDDPrimary->Blt(&renderRect, glpDDSecondary, &r, 0, NULL);
    if (FAILED(hr)) {
      NS_ERROR("this blt should never fail!");
      printf("#### %s blt failed: %08lx", __FUNCTION__, hr);
    }
  }
  result = PR_TRUE;

cleanup:
  NS_ASSERTION(result == PR_TRUE, "fatal drawing error");
  ::EndPaint(mWnd, &ps);
  mPaintDC = nsnull;
  mPainting = PR_FALSE;
  return result;

}
#endif // defined(CAIRO_HAS_DDRAW_SURFACE)
