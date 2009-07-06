/* ***** BEGIN LICENSE BLOCK *****
 * 
 * Copyright (c) 2008, Mozilla Corporation
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 * * Redistributions of source code must retain the above copyright notice, this
 *   list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 * * Neither the name of the Mozilla Corporation nor the names of its
 *   contributors may be used to endorse or promote products derived from this
 *   software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 * Contributor(s):
 *   Josh Aas <josh@mozilla.com>
 *   Jim Mathies <jmathies@mozilla.com>
 * 
 * ***** END LICENSE BLOCK ***** */

#include "nptest_platform.h"

#include <windows.h>

#pragma comment(lib, "msimg32.lib")

void SetSubclass(HWND hWnd, InstanceData* instanceData);
void ClearSubclass(HWND hWnd);
LRESULT CALLBACK PluginWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

bool
pluginSupportsWindowMode()
{
  return true;
}

bool
pluginSupportsWindowlessMode()
{
  return true;
}

NPError
pluginInstanceInit(InstanceData* instanceData)
{
  return NPERR_NO_ERROR;
}

void
pluginInstanceShutdown(InstanceData* instanceData)
{
}

void
pluginDoSetWindow(InstanceData* instanceData, NPWindow* newWindow)
{
  instanceData->window = *newWindow;
}

void
pluginWidgetInit(InstanceData* instanceData, void* oldWindow)
{
  HWND hWnd = (HWND)instanceData->window.window;
  if (oldWindow) {
    HWND hWndOld = (HWND)oldWindow;
    ClearSubclass(hWndOld);
  }
  SetSubclass(hWnd, instanceData);
}

static void
drawToDC(InstanceData* instanceData, HDC dc,
         int x, int y, int width, int height)
{
  HBITMAP offscreenBitmap = ::CreateCompatibleBitmap(dc, width, height);
  if (!offscreenBitmap)
    return;
  HDC offscreenDC = ::CreateCompatibleDC(dc);
  if (!offscreenDC) {
    ::DeleteObject(offscreenBitmap);
    return;
  }

  HBITMAP oldOffscreenBitmap =
    (HBITMAP)::SelectObject(offscreenDC, offscreenBitmap);
  ::SetBkMode(offscreenDC, TRANSPARENT);
  BYTE alpha = 255;
  RECT fill = { 0, 0, width, height };

  switch (instanceData->scriptableObject->drawMode) {
    case DM_DEFAULT:
    {
      HBRUSH brush = ::CreateSolidBrush(RGB(0, 0, 0));
      if (brush) {
        ::FillRect(offscreenDC, &fill, brush);
        ::DeleteObject(brush);
      }
      if (width > 6 && height > 6) {
        brush = ::CreateSolidBrush(RGB(192, 192, 192));
        if (brush) {
          RECT inset = { 3, 3, width - 3, height - 3 };
          ::FillRect(offscreenDC, &inset, brush);
          ::DeleteObject(brush);
        }
      }

      const char* uaString = NPN_UserAgent(instanceData->npp);
      if (uaString && width > 10 && height > 10) {
        HFONT font =
          ::CreateFontA(20, 0, 0, 0, 400, FALSE, FALSE, FALSE,
                        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                        CLIP_DEFAULT_PRECIS, 5, // CLEARTYPE_QUALITY
                        DEFAULT_PITCH, "Arial");
        if (font) {
          HFONT oldFont = (HFONT)::SelectObject(offscreenDC, font);
          RECT inset = { 5, 5, width - 5, height - 5 };
          ::DrawTextA(offscreenDC, uaString, -1, &inset,
                      DT_LEFT | DT_TOP | DT_NOPREFIX | DT_WORDBREAK);
          ::SelectObject(offscreenDC, oldFont);
          ::DeleteObject(font);
        }
      }
    }
    break;

    case DM_SOLID_COLOR:
    {
      PRUint32 rgba = instanceData->scriptableObject->drawColor;
      BYTE r = ((rgba & 0xFF0000) >> 16);
      BYTE g = ((rgba & 0xFF00) >> 8);
      BYTE b = (rgba & 0xFF);
      alpha = ((rgba & 0xFF000000) >> 24);

      HBRUSH brush = ::CreateSolidBrush(RGB(r, g, b));
      if (brush) {
        ::FillRect(offscreenDC, &fill, brush);
        ::DeleteObject(brush);
      }
    }
    break;
  }

  BLENDFUNCTION blendFunc;
  blendFunc.BlendOp = AC_SRC_OVER;
  blendFunc.BlendFlags = 0;
  blendFunc.SourceConstantAlpha = alpha;
  blendFunc.AlphaFormat = 0;
  ::AlphaBlend(dc, x, y, width, height, offscreenDC, 0, 0, width, height,
               blendFunc);
  ::SelectObject(offscreenDC, oldOffscreenBitmap);
  ::DeleteObject(offscreenDC);
  ::DeleteObject(offscreenBitmap);
}

void
pluginDraw(InstanceData* instanceData)
{
  NPP npp = instanceData->npp;
  if (!npp)
    return;

  HDC hdc = NULL;
  PAINTSTRUCT ps;

  if (instanceData->hasWidget)
    hdc = ::BeginPaint((HWND)instanceData->window.window, &ps);
  else
    hdc = (HDC)instanceData->window.window;

  if (hdc == NULL)
    return;

  // Push the browser's hdc on the resource stack. If this test plugin is windowless,
  // we share the drawing surface with the rest of the browser.
  int savedDCID = SaveDC(hdc);

  // When we have a widget, window.x/y are meaningless since our widget
  // is always positioned correctly and we just draw into it at 0,0.
  int x = instanceData->hasWidget ? 0 : instanceData->window.x;
  int y = instanceData->hasWidget ? 0 : instanceData->window.y;
  int width = instanceData->window.width;
  int height = instanceData->window.height;
  drawToDC(instanceData, hdc, x, y, width, height);

  // Pop our hdc changes off the resource stack
  RestoreDC(hdc, savedDCID);

  if (instanceData->hasWidget)
    ::EndPaint((HWND)instanceData->window.window, &ps);
}

/* script interface */

int32_t
pluginGetEdge(InstanceData* instanceData, RectEdge edge)
{
  if (!instanceData || !instanceData->hasWidget)
    return NPTEST_INT32_ERROR;

  HWND hWnd = GetAncestor((HWND)instanceData->window.window, GA_ROOT);

  if (!hWnd)
    return NPTEST_INT32_ERROR;

  RECT rect = {0};
  GetClientRect((HWND)instanceData->window.window, &rect);
  MapWindowPoints((HWND)instanceData->window.window, hWnd, (LPPOINT)&rect, 2);

  switch (edge) {
  case EDGE_LEFT:
    return rect.left;
  case EDGE_TOP:
    return rect.top;
  case EDGE_RIGHT:
    return rect.right;
  case EDGE_BOTTOM:
    return rect.bottom;
  }

  return NPTEST_INT32_ERROR;
}

int32_t
pluginGetClipRegionRectCount(InstanceData* instanceData)
{
  return 1;
}

int32_t
pluginGetClipRegionRectEdge(InstanceData* instanceData, 
    int32_t rectIndex, RectEdge edge)
{
  return pluginGetEdge(instanceData, edge);
}

/* windowless plugin events */

int16_t
pluginHandleEvent(InstanceData* instanceData, void* event)
{
  NPEvent * pe = (NPEvent*) event;

  if (pe == NULL || instanceData == NULL ||
      instanceData->window.type != NPWindowTypeDrawable)
    return 0;   

  switch((UINT)pe->event) {
    case WM_PAINT:
      pluginDraw(instanceData);   
      return 1;
  }
  
  return 0;
}

/* windowed plugin events */

LRESULT CALLBACK PluginWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	WNDPROC wndProc = (WNDPROC)GetProp(hWnd, "MozillaWndProc");
  if (!wndProc)
    return 0;
  InstanceData* pInstance = (InstanceData*)GetProp(hWnd, "InstanceData");
  if (!pInstance)
    return 0;

  if (uMsg == WM_PAINT) {
    pluginDraw(pInstance);
    return 0;
  }

  if (uMsg == WM_CLOSE) {
    ClearSubclass((HWND)pInstance->window.window);
  }

  return CallWindowProc(wndProc, hWnd, uMsg, wParam, lParam);
}

void
ClearSubclass(HWND hWnd)
{
  if (GetProp(hWnd, "MozillaWndProc")) {
    ::SetWindowLongPtr(hWnd, GWLP_WNDPROC, (LONG_PTR)GetProp(hWnd, "MozillaWndProc"));
    RemoveProp(hWnd, "MozillaWndProc");
    RemoveProp(hWnd, "InstanceData");
  }
}

void
SetSubclass(HWND hWnd, InstanceData* instanceData)
{
  // Subclass the plugin window so we can handle our own windows events.
  SetProp(hWnd, "InstanceData", (HANDLE)instanceData);
  WNDPROC origProc = (WNDPROC)::SetWindowLongPtr(hWnd, GWLP_WNDPROC, (LONG_PTR)PluginWndProc);
  SetProp(hWnd, "MozillaWndProc", (HANDLE)origProc);
}
