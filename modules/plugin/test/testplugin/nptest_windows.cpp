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
#include <unknwn.h>
#include <gdiplus.h>
using namespace Gdiplus;

#pragma comment(lib, "gdiplus.lib")

NPError
pluginInstanceInit(InstanceData* instanceData)
{
  return NPERR_NO_ERROR;
}

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

static Color
GetColorsFromRGBA(PRUint32 rgba)
{
  BYTE r, g, b, a;
  b = (rgba & 0xFF);
  g = ((rgba & 0xFF00) >> 8);
  r = ((rgba & 0xFF0000) >> 16);
  a = ((rgba & 0xFF000000) >> 24);
  return Color(a, r, g, b);
}

void
pluginDraw(InstanceData* instanceData)
{
  NPP npp = instanceData->npp;
  if (!npp)
    return;

  const char* uaString = NPN_UserAgent(npp);
  if (!uaString)
    return;

  HDC hdc = (HDC)instanceData->window.window;

  if (hdc == NULL)
    return;

  // Push the browser's hdc on the resource stack. This test plugin is windowless,
  // so we share the drawing surface with the rest of the browser.
  int savedDCID = SaveDC(hdc);

  // Reset the clipping region
  SelectClipRgn(hdc, NULL);

  // Initialize GDI+.
  GdiplusStartupInput gdiplusStartupInput;
  ULONG_PTR gdiplusToken;
  GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

  // Calculate the rectangle coordinates from the instanceData information
  Rect rect(instanceData->window.x, instanceData->window.y, 
    instanceData->window.width, instanceData->window.height);

  // Create a layout rect for the text
  RectF boundRect((float)instanceData->window.x, (float)instanceData->window.y, 
    (float)instanceData->window.width, (float)instanceData->window.height);
  boundRect.Inflate(-10.0, -10.0);

  switch(instanceData->scriptableObject->drawMode) {
    case DM_DEFAULT:
    {
      Graphics graphics(hdc);

      // Fill in the background and border
      Pen blackPen(Color(255, 0, 0, 0), 5);
      SolidBrush grayBrush(Color(255, 192, 192, 192));

      graphics.FillRectangle(&grayBrush, rect);
      graphics.DrawRectangle(&blackPen, rect);

      // Load a nice font
      FontFamily fontFamily(L"Helvetica");
      Font font(&fontFamily, 20, FontStyleBold, UnitPoint);
      StringFormat stringFormat;
      SolidBrush solidBrush(Color(255, 0, 0, 0));

      // Center the text string
      stringFormat.SetAlignment(StringAlignmentCenter);
      stringFormat.SetLineAlignment(StringAlignmentCenter);

      // Request anti-aliased text
      graphics.SetTextRenderingHint(TextRenderingHintAntiAlias);

      WCHAR wBuf[1024];
      memset(&wBuf, 0, sizeof(wBuf));
      MultiByteToWideChar(CP_ACP, 0, uaString, -1, wBuf, 1024);

      // Draw the string
      graphics.DrawString(wBuf, -1, &font, boundRect, &stringFormat, &solidBrush);
    }
    break;

    case DM_SOLID_COLOR:
    {
      // Fill the plugin window with a solid color specified by the params
      Graphics graphics(hdc);
      SolidBrush brush(GetColorsFromRGBA(instanceData->scriptableObject->drawColor));
      graphics.FillRectangle(&brush, rect.X, rect.Y, rect.Width, rect.Height);
    }
    break;
  }

  // Shutdown GDI+
  GdiplusShutdown(gdiplusToken);

  // Pop our hdc changes off the resource stack
  RestoreDC(hdc, savedDCID);
}
