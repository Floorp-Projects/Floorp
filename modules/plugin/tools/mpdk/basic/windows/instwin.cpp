/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *
 */

/**********************************************************************
*
* instwin.cpp
*
* Implementation of the plugin instance class for Windows
* 
***********************************************************************/

#include <windows.h>
#include <windowsx.h>

#include "instwin.h"
#include "dbg.h"

CPluginInstance * Platform_CreatePluginInstance(nsISupports *aOuter, REFNSIID aIID, const char* aPluginMIMEType)
{
  CPluginInstanceWin *inst = new CPluginInstanceWin();
  return (CPluginInstance *)inst;
}

/*------------------------------------
 * CPluginInstanceWin implementation
 *-----------------------------------*/

CPluginInstanceWin::CPluginInstanceWin() : CPluginInstance()
{
  dbgOut1("CPluginInstanceWin::CPluginInstanceWin");
  platform = nsnull;
}

CPluginInstanceWin::~CPluginInstanceWin()
{
  dbgOut1("CPluginInstanceWin::~CPluginInstanceWin");
}

nsresult CPluginInstanceWin::PlatformNew()
{
  dbgOut1("CPluginInstanceWin::PlatformNew");

  platform = new CPlatformWin();

  if(platform == nsnull)
    return NS_ERROR_OUT_OF_MEMORY;

  return NS_OK;
}

nsresult CPluginInstanceWin::PlatformDestroy()
{
  dbgOut1("CPluginInstanceWin::PlatformDestroy");

  if(platform != nsnull)
  {
    delete platform;
    platform = nsnull;
  }

  return NS_OK;
}

nsresult CPluginInstanceWin::PlatformSetWindow(nsPluginWindow* window)
{
  dbgOut1("CPluginInstanceWin::PlatformSetWindow");

  if(fWindow == nsnull) // we do not have a window yet
  {
    // platform should not be initialized if(fWindow == nsnull) but...
    if(platform->m_bInitialized)
      platform->shut();

    if(window == nsnull) // not very meaningful if(fWindow == nsnull)
      return NS_OK;

    if(window->window != nsnull) // spurious entry
      return platform->init(window);
    else                         // First time in
      return NS_OK;
  }

  else // (fWindow != nsnull), we already have a window
  
  {
    if((window == nsnull) || (window->window == nsnull)) // window went away
    {
      if(platform->m_bInitialized)
        platform->shut();
      return NS_OK;
    }

    if(!platform->m_bInitialized) // First time in
      return platform->init(window);
    else
    { // Netscape window has been resized or changed
      if(platform->m_hWnd != (HWND)window->window) // changed
      {
        platform->shut();
        return platform->init(window);
      }
      else // resized?
        return NS_OK;
    }
  }

  return NS_OK;
}

PRInt16 CPluginInstanceWin::PlatformHandleEvent(nsPluginEvent* event)
{
  dbgOut1("CPluginInstanceWin::PlatformHandleEvent");
  return 0;
}

/*------------------------------------
 * CPlatformWin implementation
 *-----------------------------------*/

static LRESULT CALLBACK PluginWindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

CPlatformWin::CPlatformWin() :
  m_bInitialized(PR_FALSE),
  m_hWnd(nsnull),
  m_OldWindowProc(nsnull)
{
  dbgOut1("CPlatformWin::CPlatformWin");
}

CPlatformWin::~CPlatformWin()
{
  dbgOut1("CPlatformWin::~CPlatformWin");
}

nsresult CPlatformWin::init(nsPluginWindow * window)
{
  dbgOut1("CPlatformWin::init");

  m_hWnd = (HWND)window->window;

  if(!IsWindow(m_hWnd))
    return NS_ERROR_UNEXPECTED;

  m_OldWindowProc = SubclassWindow(m_hWnd, (WNDPROC)PluginWindowProc);
  m_bInitialized = PR_TRUE;
  return NS_OK;
}

void CPlatformWin::shut()
{
  dbgOut1("CPlatformWin::shut");

  if(m_hWnd != nsnull)
  {
    SubclassWindow(m_hWnd, m_OldWindowProc);
    m_OldWindowProc = nsnull;
    m_hWnd = nsnull;
  }
  m_bInitialized = PR_FALSE;
}

/*------------------------------------
 * Window message handlers
 *-----------------------------------*/

static void onPaint(HWND hWnd)
{
  PAINTSTRUCT ps;
  HDC hDC = BeginPaint(hWnd, &ps);
  HBRUSH hBrush = CreateSolidBrush(RGB(0,255,0));
  HBRUSH hBrushOld = SelectBrush(hDC, hBrush);
  RECT rc;
  GetClientRect(hWnd, &rc);
  FillRect(hDC, &rc, hBrush);
  SelectBrush(hDC, hBrushOld);
  DeleteBrush(hBrush);
  EndPaint(hWnd, &ps);
}

static void onDestroyWindow(HWND hWnd)
{
  dbgOut1("onDestroyWindow");
}

static LRESULT CALLBACK PluginWindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
  switch (msg)
  {
    case WM_PAINT:
      HANDLE_WM_PAINT(hWnd, wParam, lParam, onPaint);
      return 0L;
    case WM_DESTROY:
      HANDLE_WM_DESTROY(hWnd, wParam, lParam, onDestroyWindow);
      break;
    default:
      break;
  }

  return DefWindowProc(hWnd, msg, wParam, lParam);
}
