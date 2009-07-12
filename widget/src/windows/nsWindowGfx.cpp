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

#ifndef WINCE
#include "nsUXThemeData.h"
#include "nsUXThemeConstants.h"
#endif

extern "C" {
#include "pixman.h"
}

#ifdef DEBUG_vladimir
#include "nsFunctionTimer.h"
#endif

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

nsAutoPtr<PRUint8>  nsWindow::sSharedSurfaceData;
gfxIntSize          nsWindow::sSharedSurfaceSize;

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

    gfxIntSize oldSize = nsWindow::sSharedSurfaceSize;
    nsWindow::sSharedSurfaceSize.height = GetSystemMetrics(SM_CYSCREEN);
    nsWindow::sSharedSurfaceSize.width = GetSystemMetrics(SM_CXSCREEN);

    // if the area is different, reallocate during WM_PAINT.
    if (nsWindow::sSharedSurfaceSize.height * nsWindow::sSharedSurfaceSize.width !=
        oldSize.height * oldSize.width)
      nsWindow::sSharedSurfaceData = nsnull;

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

PRBool nsWindow::OnPaint(HDC aDC)
{
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
  HRGN paintRgn = NULL;

#ifdef MOZ_XUL
  if (aDC || (eTransparencyTransparent == mTransparencyMode)) {
#else
  if (aDC) {
#endif

    RECT paintRect;
    ::GetClientRect(mWnd, &paintRect);
    paintRgn = ::CreateRectRgn(paintRect.left, paintRect.top, paintRect.right, paintRect.bottom);
  }
  else {
#ifndef WINCE
    paintRgn = ::CreateRectRgn(0, 0, 0, 0);
    if (paintRgn != NULL) {
      int result = GetRandomRgn(hDC, paintRgn, SYSRGN);
      if (result == 1) {
        POINT pt = {0,0};
        ::MapWindowPoints(NULL, mWnd, &pt, 1);
        ::OffsetRgn(paintRgn, pt.x, pt.y);
      }
    }
#else
    paintRgn = ::CreateRectRgn(ps.rcPaint.left, ps.rcPaint.top, 
                               ps.rcPaint.right, ps.rcPaint.bottom);
#endif
  }

#ifdef DEBUG_vladimir
  nsFunctionTimer ft("OnPaint [%d %d %d %d]",
                     ps.rcPaint.left, ps.rcPaint.top, 
                     ps.rcPaint.right - ps.rcPaint.left,
                     ps.rcPaint.bottom - ps.rcPaint.top);
#endif

  nsCOMPtr<nsIRegion> paintRgnWin;
  if (paintRgn) {
    paintRgnWin = nsWindowGfx::ConvertHRGNToRegion(paintRgn);
    ::DeleteObject(paintRgn);
  }

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
      if (!sSharedSurfaceData) {
        sSharedSurfaceSize.height = GetSystemMetrics(SM_CYSCREEN);
        sSharedSurfaceSize.width = GetSystemMetrics(SM_CXSCREEN);
        sSharedSurfaceData = (PRUint8*) malloc(sSharedSurfaceSize.width * sSharedSurfaceSize.height * 4);
      }

      gfxIntSize surfaceSize(ps.rcPaint.right - ps.rcPaint.left,
                             ps.rcPaint.bottom - ps.rcPaint.top);

      if (!sSharedSurfaceData ||
          surfaceSize.width > sSharedSurfaceSize.width ||
          surfaceSize.height > sSharedSurfaceSize.height)
      {
        // allocate a new oversize surface; hopefully this will just be a one-time thing,
        // and we should really fix whatever's doing it!
        targetSurfaceImage = new gfxImageSurface(surfaceSize, gfxASurface::ImageFormatRGB24);
      } else {
        // don't use the shared surface directly; instead, create a new one
        // that just reuses its buffer.
        targetSurfaceImage = new gfxImageSurface(sSharedSurfaceData.get(),
                                                 surfaceSize,
                                                 surfaceSize.width * 4,
                                                 gfxASurface::ImageFormatRGB24);
      }

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

#ifdef DEBUG_vladimir
    ft.Mark("Init");
#endif

    event.renderingContext = rc;
    result = DispatchWindowEvent(&event, eventStatus);
    event.renderingContext = nsnull;

#ifdef DEBUG_vladimir
    ft.Mark("Dispatch");
#endif

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

#ifdef DEBUG_vladimir
      ft.Mark("Blit");
#endif
    } else {
#ifdef DEBUG_vladimir
      ft.Mark("Discard!");
#endif
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

// Windows Mobile Special image/direct draw painting fun
#if defined(CAIRO_HAS_DDRAW_SURFACE)
PRBool nsWindow::OnPaintImageDDraw16()
{
  PRBool result = PR_TRUE;
  PAINTSTRUCT ps;
  gfxIntSize surfaceSize;
  nsPaintEvent event(PR_TRUE, NS_PAINT, this);
  RECT renderArea;

  nsEventStatus eventStatus = nsEventStatus_eIgnore;
  nsCOMPtr<nsIRenderingContext> rc;
  nsRefPtr<gfxImageSurface> targetSurfaceImage;
  nsRefPtr<gfxContext> thebesContext;

  mPainting = PR_TRUE;

  HDC hDC = ::BeginPaint(mWnd, &ps);
  mPaintDC = hDC;

  HRGN paintRgn = ::CreateRectRgn(ps.rcPaint.left, ps.rcPaint.top, 
                                  ps.rcPaint.right, ps.rcPaint.bottom);
#ifdef DEBUG_vladimir
  nsFunctionTimer ft("OnPaint [%d %d %d %d]",
                     ps.rcPaint.left, ps.rcPaint.top, 
                     ps.rcPaint.right - ps.rcPaint.left,
                     ps.rcPaint.bottom - ps.rcPaint.top);
#endif

  nsCOMPtr<nsIRegion> paintRgnWin;
  if (paintRgn) {
    paintRgnWin = nsWindowGfx::ConvertHRGNToRegion(paintRgn);
    ::DeleteObject(paintRgn);
  }

  if (!paintRgnWin || paintRgnWin->IsEmpty() || !mEventCallback) {
    printf("nothing to paint\n");
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
  
  if (!sSharedSurfaceData) {
    sSharedSurfaceSize.height = GetSystemMetrics(SM_CYSCREEN);
    sSharedSurfaceSize.width = GetSystemMetrics(SM_CXSCREEN);
    sSharedSurfaceData = (PRUint8*) malloc(sSharedSurfaceSize.width * sSharedSurfaceSize.height * 4);
  }

  if (!glpDDSecondary) {

    memset(&gDDSDSecondary, 0, sizeof (gDDSDSecondary));
    memset(&gDDSDSecondary.ddpfPixelFormat, 0, sizeof(gDDSDSecondary.ddpfPixelFormat));
    
    gDDSDSecondary.dwSize = sizeof (gDDSDSecondary);
    gDDSDSecondary.ddpfPixelFormat.dwSize = sizeof(gDDSDSecondary.ddpfPixelFormat);
    
    gDDSDSecondary.dwFlags = DDSD_HEIGHT | DDSD_WIDTH | DDSD_PIXELFORMAT;

    gDDSDSecondary.dwHeight = sSharedSurfaceSize.height;
    gDDSDSecondary.dwWidth  = sSharedSurfaceSize.width;

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

  surfaceSize = gfxIntSize(ps.rcPaint.right - ps.rcPaint.left,
                           ps.rcPaint.bottom - ps.rcPaint.top);

  targetSurfaceImage = new gfxImageSurface(sSharedSurfaceData.get(),
					   surfaceSize,
					   surfaceSize.width * 4,
					   gfxASurface::ImageFormatRGB24);

  if (!targetSurfaceImage || targetSurfaceImage->CairoStatus()) {
    NS_ERROR("Invalid targetSurfaceImage!");
    goto cleanup;
  }

  targetSurfaceImage->SetDeviceOffset(gfxPoint(-ps.rcPaint.left, -ps.rcPaint.top));

  thebesContext = new gfxContext(targetSurfaceImage);
  thebesContext->SetFlag(gfxContext::FLAG_DESTINED_FOR_SCREEN);
  thebesContext->SetFlag(gfxContext::FLAG_SIMPLIFY_OPERATORS);
  
  nsresult rv = mContext->CreateRenderingContextInstance (*getter_AddRefs(rc));
  if (NS_FAILED(rv)) {
    NS_WARNING("CreateRenderingContextInstance failed");
    goto cleanup;
  }
  
  rv = rc->Init(mContext, thebesContext);
  if (NS_FAILED(rv)) {
    NS_WARNING("RC::Init failed");
    goto cleanup;
  }
  
#ifdef DEBUG_vladimir
  ft.Mark("Init");
#endif
  event.renderingContext = rc;
  result = DispatchWindowEvent(&event, eventStatus);
  event.renderingContext = nsnull;
  
#ifdef DEBUG_vladimir
  ft.Mark("Dispatch");
#endif
  
  if (!result) {
    printf("result is null from dispatch\n");
    goto cleanup;
  }
    
  HRESULT hr;  
  hr = glpDDSecondary->Lock(0, &gDDSDSecondary, DDLOCK_WAITNOTBUSY | DDLOCK_DISCARD, 0);  /* should we wait here? */
  if (FAILED(hr)) {
#ifdef DEBUG
    DDError("Failed to lock renderer", hr);
#endif
    goto cleanup;
  }
  
#ifdef DEBUG_vladimir
  ft.Mark("Locked");
#endif
  // Convert RGB24 -> RGB565
  pixman_image_t *srcPixmanImage = pixman_image_create_bits(PIXMAN_x8r8g8b8,
                                                            surfaceSize.width,
                                                            surfaceSize.height,
                                                            (uint32_t*) sSharedSurfaceData.get(),
                                                            surfaceSize.width * 4);
  
  pixman_image_t *dstPixmanImage = pixman_image_create_bits(PIXMAN_r5g6b5,
                                                            surfaceSize.width,
                                                            surfaceSize.height,
                                                            (uint32_t*) gDDSDSecondary.lpSurface,
                                                            gDDSDSecondary.dwWidth * 2);
 
#ifdef DEBUG_vladimir
  ft.Mark("created pixman images");
#endif

  pixman_image_composite(PIXMAN_OP_SRC,
                         srcPixmanImage,
                         NULL,
                         dstPixmanImage,
                         0, 0,
                         0, 0,
                         0, 0,
                         surfaceSize.width,
                         surfaceSize.height);

  pixman_image_unref(dstPixmanImage);
  pixman_image_unref(srcPixmanImage);

#ifdef DEBUG_vladimir
  ft.Mark("composite");
#endif
  
  hr = glpDDSecondary->Unlock(0);
  if (FAILED(hr)) {
#ifdef DEBUG
    DDError("Failed to unlock renderer", hr);
#endif
    goto cleanup;
  }

#ifdef DEBUG_vladimir
  ft.Mark("unlock");
#endif

  hr = glpDDClipper->SetHWnd(0, mWnd);
  if (FAILED(hr)) {
#ifdef DEBUG
    DDError("SetHWnd", hr);
#endif
    goto cleanup;
  }

#ifdef DEBUG_vladimir
  ft.Mark("sethwnd");
#endif

  // translate the paint region to screen coordinates
  renderArea = ps.rcPaint;
  MapWindowPoints(mWnd, 0, (LPPOINT)&renderArea, 2);

#ifdef DEBUG_vladimir
  ft.Mark("preblt");
#endif

  // set the rect to be 0,0 based
  ps.rcPaint.right = surfaceSize.width;
  ps.rcPaint.bottom = surfaceSize.height;
  ps.rcPaint.left = ps.rcPaint.top = 0;
  
  hr = glpDDPrimary->Blt(&renderArea,
                         glpDDSecondary,
                         &ps.rcPaint,
                         DDBLT_WAITNOTBUSY, /* should we really wait here? */
                         NULL);
  if (FAILED(hr)) {
#ifdef DEBUG
    DDError("Blt", hr);
#endif
    goto cleanup;
  }

#ifdef DEBUG_vladimir
  ft.Mark("Blit");
#endif

cleanup:
 
  ::EndPaint(mWnd, &ps);
  mPaintDC = nsnull;
  mPainting = PR_FALSE;
  return result;
}
#endif // defined(CAIRO_HAS_DDRAW_SURFACE)
