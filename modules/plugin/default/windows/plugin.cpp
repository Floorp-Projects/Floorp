/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include <windows.h>
#include <windowsx.h>

#include "resource.h"

#include "plugin.h"
#include "utils.h"
#include "dialogs.h"
#include "dbg.h"

static char szNullPluginWindowClassName[] = CLASS_NULL_PLUGIN;

static LRESULT CALLBACK NP_LOADDS PluginWndProc(HWND, UINT, WPARAM, LPARAM);

static char szDefaultPluginFinderURL[] = DEFAULT_PLUGINFINDER_URL;
static char szPageUrlForJavaScript[] = PAGE_URL_FOR_JAVASCRIPT;
static char szPageUrlForJVM[] = JVM_SMARTUPDATE_URL;
//static char szPluginFinderCommandFormatString[] = PLUGINFINDER_COMMAND;
static char szPluginFinderCommandBeginning[] = PLUGINFINDER_COMMAND_BEGINNING;
static char szPluginFinderCommandEnd[] = PLUGINFINDER_COMMAND_END;
static char szDefaultFileExt[] = "*";

BOOL RegisterNullPluginWindowClass()
{
  assert(hInst != NULL);

  WNDCLASS wc;

  memset(&wc, 0, sizeof(wc));

  wc.lpfnWndProc   = (WNDPROC)PluginWndProc;
  wc.cbWndExtra    = sizeof(DWORD);
  wc.hInstance     = hInst;
  wc.hIcon         = LoadIcon(hInst, IDI_APPLICATION);
  wc.hCursor       = NULL;
  wc.hbrBackground = HBRUSH(COLOR_WINDOW + 1);
  wc.lpszClassName = szNullPluginWindowClassName;

  ATOM aRet = RegisterClass(&wc);
  return (aRet != NULL);
}

void UnregisterNullPluginWindowClass()
{
  assert(hInst != NULL);
  UnregisterClass(szNullPluginWindowClassName, hInst);
}

/*********************************************/
/*                                           */
/*       CPlugin class implementation        */
/*                                           */
/*********************************************/

CPlugin::CPlugin(HINSTANCE hInst, 
                 NPP pNPInstance, 
                 WORD wMode, 
                 NPMIMEType pluginType, 
                 LPSTR szPageURL, 
                 LPSTR szFileURL, 
                 LPSTR szFileExtension,
                 BOOL bHidden) :
  m_pNPInstance(pNPInstance),
  m_wMode(wMode),
  m_hInst(hInst),
  m_hWnd(NULL),
  m_hWndParent(NULL),
  m_hWndDialog(NULL),
  m_hIcon(NULL),
  m_pNPMIMEType(NULL),
  m_szPageURL(NULL),
  m_szFileURL(NULL),
  m_szFileExtension(NULL),
  m_bOnline(TRUE),
  m_bJava(TRUE),
  m_bJavaScript(TRUE),
  m_bSmartUpdate(TRUE),
  m_szURLString(NULL),
  m_szCommandMessage(NULL),
  m_bHidden(bHidden)
{
  dbgOut1("CPlugin::CPlugin()");
  assert(m_hInst != NULL);
  assert(m_pNPInstance != NULL);

  if((pluginType != NULL) && (lstrlen((LPSTR)pluginType) != 0))
  {
    m_pNPMIMEType = (NPMIMEType)new char[lstrlen((LPSTR)pluginType) + 1];
    if(m_pNPMIMEType != NULL)
      lstrcpy((LPSTR)m_pNPMIMEType, pluginType);
  }

  if((szPageURL != NULL) && (lstrlen(szPageURL) != 0))
  {
    m_szPageURL = new char[lstrlen(szPageURL) + 1];
    if(m_szPageURL != NULL)
      lstrcpy(m_szPageURL, szPageURL);
  }
  
  if((szFileURL != NULL) && (lstrlen(szFileURL) != 0))
  {
    m_szFileURL = new char[lstrlen(szFileURL) + 1];
    if(m_szFileURL != NULL)
      lstrcpy(m_szFileURL, szFileURL);
  }

  if((szFileExtension != NULL) && (lstrlen(szFileExtension) != 0))
  {
    m_szFileExtension = new char[lstrlen(szFileExtension) + 1];
    if(m_szFileExtension != NULL)
      lstrcpy(m_szFileExtension, szFileExtension);
  }

  m_hIcon = LoadIcon(m_hInst, MAKEINTRESOURCE(IDI_PLUGICON));

  char szString[1024] = {'\0'};
  LoadString(m_hInst, IDS_CLICK_TO_GET, szString, sizeof(szString));
  if(lstrlen(szString) != 0)
  {
    m_szCommandMessage = new char[lstrlen(szString) + 1];
    if(m_szCommandMessage != NULL)
      lstrcpy(m_szCommandMessage, szString);
  }
}

CPlugin::~CPlugin()
{
  dbgOut1("CPlugin::~CPlugin()");

  if(m_pNPMIMEType != NULL)
  {
    delete [] m_pNPMIMEType;
    m_pNPMIMEType = NULL;
  }

  if(m_szPageURL != NULL)
  {
    delete [] m_szPageURL;
    m_szPageURL = NULL;
  }

  if(m_szFileURL != NULL)
  {
    delete [] m_szFileURL;
    m_szFileURL = NULL;
  }

  if(m_szFileExtension != NULL)
  {
    delete [] m_szFileExtension;
    m_szFileExtension = NULL;
  }

  if(m_szURLString != NULL)
  {
    delete [] m_szURLString;
    m_szURLString = NULL;
  }

  if(m_hIcon != NULL)
  {
    DestroyIcon(m_hIcon);
    m_hIcon = NULL;
  }

  if(m_szCommandMessage != NULL)
  {
    delete [] m_szCommandMessage;
    m_szCommandMessage = NULL;
  }
}

BOOL CPlugin::init(HWND hWndParent)
{
  dbgOut1("CPlugin::init()");

  if(!m_bHidden)
  {
    assert(IsWindow(hWndParent));

    if(IsWindow(hWndParent))
      m_hWndParent = hWndParent;

    RECT rcParent;
    GetClientRect(m_hWndParent, &rcParent);

    CreateWindow(szNullPluginWindowClassName, 
                 "NULL Plugin", 
                 WS_CHILD,
                 0,0, rcParent.right, rcParent.bottom,
                 m_hWndParent,
                 (HMENU)NULL,
                 m_hInst,
                 (LPVOID)this);

    assert(m_hWnd != NULL);
    if((m_hWnd == NULL) || (!IsWindow(m_hWnd)))
      return FALSE;

    UpdateWindow(m_hWnd);
    ShowWindow(m_hWnd, SW_SHOW);
  }

  if(IsNewMimeType((LPSTR)m_pNPMIMEType) || m_bHidden)
    showGetPluginDialog();

  return TRUE;
}

void CPlugin::shut()
{
  dbgOut1("CPlugin::shut()");

  if(m_hWndDialog != NULL)
  {
    DestroyWindow(m_hWndDialog);
    m_hWndDialog = NULL;
  }

  if(m_hWnd != NULL)
  {
    DestroyWindow(m_hWnd);
    m_hWnd = NULL;
  }
}

HWND CPlugin::getWindow()
{
  return m_hWnd;
}

BOOL CPlugin::useDefaultURL_P()
{
  if((m_szPageURL == NULL) && (m_szFileURL == NULL))
    return TRUE;
  else
    return FALSE;
}

BOOL CPlugin::doSmartUpdate_P()
{
  // due to current JavaScript problems never do it smart for now 5.1.98
  return FALSE;

  if(m_bOnline && m_bJava && m_bJavaScript && m_bSmartUpdate && useDefaultURL_P())
    return TRUE;
  else
    return FALSE;
}

LPSTR CPlugin::createURLString()
{
  if(m_szURLString != NULL)
  {
    delete [] m_szURLString;
    m_szURLString = NULL;
  }

  // check if there is file URL first
  if(m_szFileURL != NULL)
  {
    m_szURLString = new char[lstrlen(m_szFileURL) + 1];
    if(m_szURLString == NULL)
      return NULL;
    lstrcpy(m_szURLString, m_szFileURL);
    return m_szURLString;
  }
  
  // if not get the page URL
  char * szAddress = NULL;
  char *urlToOpen = NULL;
  char contentTypeIsJava = 0;

  if (NULL != m_pNPMIMEType) {
    contentTypeIsJava = (0 == strcmp("application/x-java-vm",
				     m_pNPMIMEType)) ? 1 : 0;
  }
  
  if(m_szPageURL != NULL && !contentTypeIsJava)
  {
    szAddress = new char[lstrlen(m_szPageURL) + 1];
    if(szAddress == NULL)
      return NULL;
    lstrcpy(szAddress, m_szPageURL);

    m_szURLString = new char[lstrlen(szAddress) + 1 + lstrlen((LPSTR)m_pNPMIMEType) + 1];

    if(m_szURLString == NULL)
      return NULL;

    // Append the MIME type to the URL
    wsprintf(m_szURLString, "%s?%s", szAddress, (LPSTR)m_pNPMIMEType);
  }
  else // default
  {
    if(!m_bJavaScript)
    {
      urlToOpen = szDefaultPluginFinderURL;
      
      if (contentTypeIsJava) {
        urlToOpen = szPageUrlForJVM;
      }

      szAddress = new char[lstrlen(urlToOpen) + 1];
      if(szAddress == NULL)
        return NULL;
      lstrcpy(szAddress, urlToOpen);

      m_szURLString = new char[lstrlen(szAddress) + 1 + lstrlen((LPSTR)m_pNPMIMEType) + 1];

      if(m_szURLString == NULL)
        return NULL;

      // Append the MIME type to the URL
      wsprintf(m_szURLString, "%s?%s", szAddress, (LPSTR)m_pNPMIMEType);
    }
    else
    {
      urlToOpen = szPageUrlForJavaScript;

      if (contentTypeIsJava) {
	urlToOpen = szPageUrlForJVM;
      }

      m_szURLString = new char[lstrlen(szPluginFinderCommandBeginning) + lstrlen(urlToOpen) + 10 + 
                               lstrlen((LPSTR)m_pNPMIMEType) + lstrlen(szPluginFinderCommandEnd) + 1];
      wsprintf(m_szURLString, "%s%s?mimetype=%s%s",
               szPluginFinderCommandBeginning, urlToOpen, 
               (LPSTR)m_pNPMIMEType, szPluginFinderCommandEnd);
    }
  }

  if(szAddress != NULL)
    delete [] szAddress;

  return m_szURLString;
}

void CPlugin::getPluginRegular()
{
  assert(m_bOnline);

  char * szURL = createURLString();

  assert(szURL != NULL);
  if(szURL == NULL)
    return;

  dbgOut3("CPlugin::getPluginRegular(), %#08x '%s'", m_pNPInstance, szURL);

  if(m_szFileURL != NULL)
    NPN_GetURL(m_pNPInstance, szURL, "_current");
  else if(m_szPageURL != NULL)
    NPN_GetURL(m_pNPInstance, szURL, "_blank");
  else if(m_bJavaScript)
    NPN_GetURL(m_pNPInstance, szURL, NULL);
  else
    NPN_GetURL(m_pNPInstance, szURL, "_blank");
}

void CPlugin::getPluginSmart()
{
/*
  static char szJSString[2048];

  wsprintf(szJSString, 
           szPluginFinderCommandFormatString, 
           szDefaultPluginFinderURL, 
           m_pNPMIMEType, 
           (m_szFileExtension != NULL) ? m_szFileExtension : szDefaultFileExt);

  dbgOut3("%#08x '%s'", m_pNPInstance, szJSString);

  assert(lstrlen(szJSString) > 0);

  NPN_GetURL(m_pNPInstance, szJSString, "smartupdate_plugin_finder");
*/
}

void CPlugin::showGetPluginDialog()
{
  assert(m_pNPMIMEType != NULL);
  if(m_pNPMIMEType == NULL)
    return;

  // Get environment
  BOOL bOffline = FALSE;

  NPN_GetValue(m_pNPInstance, NPNVisOfflineBool, (void *)&bOffline);
  NPN_GetValue(m_pNPInstance, NPNVjavascriptEnabledBool, (void *)&m_bJavaScript);
  NPN_GetValue(m_pNPInstance, NPNVasdEnabledBool, (void *)&m_bSmartUpdate);

  m_bOnline = !bOffline;

  if(m_bOnline && m_bJavaScript && m_bSmartUpdate && useDefaultURL_P())
  {
    JRIEnv *penv = NPN_GetJavaEnv();
    m_bJava = (penv != NULL);
  }
  
  dbgOut1("Environment:");
  dbgOut2("%s", m_bOnline ? "On-line" : "Off-line");
  dbgOut2("Java %s", m_bJava ? "Enabled" : "Disabled");
  dbgOut2("JavaScript %s", m_bJavaScript ? "Enabled" : "Disabled");
  dbgOut2("SmartUpdate %s", m_bSmartUpdate ? "Enabled" : "Disabled");

  if((m_szPageURL != NULL) || (m_szFileURL != NULL) || !m_bJavaScript)
  {
    // we don't want it more than once
    if(m_hWndDialog == NULL)
      CreateDialogParam(m_hInst, MAKEINTRESOURCE(IDD_PLUGIN_DOWNLOAD), m_hWnd,
                        (DLGPROC)GetPluginDialogProc, (LPARAM)this);
  }
  else
    getPlugin();
}

void CPlugin::getPlugin()
{
  if(m_szCommandMessage != NULL)
  {
    delete [] m_szCommandMessage;
    m_szCommandMessage = NULL;
  }

  char szString[1024] = {'\0'};
  LoadString(m_hInst, IDS_CLICK_WHEN_DONE, szString, sizeof(szString));
  if(lstrlen(szString) != 0)
  {
    m_szCommandMessage = new char[lstrlen(szString) + 1];
    if(m_szCommandMessage != NULL)
      lstrcpy(m_szCommandMessage, szString);
  }

  InvalidateRect(m_hWnd, NULL, TRUE);
  UpdateWindow(m_hWnd);

  getPluginRegular();
}

//*******************
// NP API handles
//*******************
void CPlugin::resize()
{
  dbgOut1("CPlugin::resize()");
}

void CPlugin::print(NPPrint * pNPPrint)
{
  dbgOut1("CPlugin::print()");

  if(pNPPrint == NULL)
    return;
}

void CPlugin::URLNotify(const char * szURL)
{
  dbgOut2("CPlugin::URLNotify(), URL '%s'", szURL);

  NPStream * pStream = NULL;
  char buf[256];

  assert(m_hInst != NULL);
  assert(m_pNPInstance != NULL);
  
  int iSize = LoadString(m_hInst, IDS_GOING2HTML, buf, sizeof(buf));

  NPError rc = NPN_NewStream(m_pNPInstance, "text/html", "asd_plugin_finder", &pStream);

  //char buf[] = "<html>\n<body>\n\n<h2 align=center>NPN_NewStream / NPN_Write - This seems to work.</h2>\n\n</body>\n</html>";
  
  int32 iBytes = NPN_Write(m_pNPInstance, pStream, lstrlen(buf), buf);

  NPN_DestroyStream(m_pNPInstance, pStream, NPRES_DONE);
}

BOOL CPlugin::readyToRefresh()
{
  char szString[1024] = {'\0'};
  LoadString(m_hInst, IDS_CLICK_WHEN_DONE, szString, sizeof(szString));
  if(m_szCommandMessage == NULL)
    return FALSE;

  return (lstrcmp(m_szCommandMessage, szString) == 0);
}

//***************************
// Windows message handlers
//***************************
void CPlugin::onCreate(HWND hWnd)
{
  m_hWnd = hWnd;
}

void CPlugin::onLButtonUp(HWND hWnd, int x, int y, UINT keyFlags)
{
  if(!readyToRefresh())
    showGetPluginDialog();
  else
    NPN_GetURL(m_pNPInstance, "javascript:navigator.plugins.refresh(true)", "_self");
}

void CPlugin::onRButtonUp(HWND hWnd, int x, int y, UINT keyFlags)
{
  if(!readyToRefresh())
    showGetPluginDialog();
  else
    NPN_GetURL(m_pNPInstance, "javascript:navigator.plugins.refresh(true)", "_self");
}

static void DrawCommandMessage(HDC hDC, LPSTR szString, LPRECT lprc)
{
  if(szString == NULL)
    return;

  HFONT hFont = GetStockFont(DEFAULT_GUI_FONT);
  if(hFont == NULL)
    return;

  HFONT hFontOld = SelectFont(hDC, hFont);
  SIZE sz;
  GetTextExtentPoint32(hDC, szString, lstrlen(szString), &sz);
  POINT pt;
  pt.x = sz.cx;
  pt.y = sz.cy;
  LPtoDP(hDC, &pt, 1);

  int iY = (lprc->bottom / 2) - ((32) / 2) + 36;
  int iX = 0;

  if(lprc->right > pt.x)
    iX = lprc->right/2 - pt.x/2;
  else
    iX = 1;

  RECT rcText;
  rcText.left   = iX;
  rcText.right  = rcText.left + pt.x;
  rcText.top    = iY;
  rcText.bottom = rcText.top + pt.y;

  int iModeOld = SetBkMode(hDC, TRANSPARENT);
  COLORREF crColorOld = SetTextColor(hDC, RGB(0,0,0));
  DrawText(hDC, szString, lstrlen(szString), &rcText, DT_CENTER|DT_VCENTER);
  SetTextColor(hDC, crColorOld);
  SetBkMode(hDC, iModeOld);
  SelectFont(hDC, hFontOld);
}

void CPlugin::onPaint(HWND hWnd)
{
  RECT rc;
  HDC hDC;
  PAINTSTRUCT ps;
  int x, y;

  hDC = BeginPaint(hWnd, &ps);
  GetClientRect(hWnd, &rc);

  x = (rc.right / 2) - (32 / 2);
  y = (rc.bottom / 2) - ((32) / 2);

  if(rc.right > 34 && rc.bottom > 34)
  {
    if(m_hIcon != NULL)
      DrawIcon(hDC, x, y, m_hIcon);

    POINT pt[5];

    // left vert and top horiz highlight
    pt[0].x = 1;            pt[0].y = rc.bottom-1;
    pt[1].x = 1;            pt[1].y = 1;
    pt[2].x = rc.right-1; pt[2].y = 1;

    HPEN hPen3DLight  = CreatePen(PS_SOLID, 1, GetSysColor(COLOR_3DLIGHT));
    HPEN hPen3DShadow = CreatePen(PS_SOLID, 1, GetSysColor(COLOR_3DSHADOW));
    HPEN hPen3DDKShadow = CreatePen(PS_SOLID, 1, GetSysColor(COLOR_3DDKSHADOW));

    HPEN hPenOld = SelectPen(hDC, hPen3DLight);
    Polyline(hDC, pt, 3);

    // left vert and top horiz shadow
    pt[0].x = 2;            pt[0].y = rc.bottom-3;
    pt[1].x = 2;            pt[1].y = 2;
    pt[2].x = rc.right-2; pt[2].y = 2;
    SelectPen(hDC, hPen3DShadow);
    Polyline(hDC, pt, 3);

    // right vert and bottom horiz highlight
    pt[0].x = rc.right-3; pt[0].y = 2;
    pt[1].x = rc.right-3; pt[1].y = rc.bottom-3;
    pt[2].x = 2;            pt[2].y = rc.bottom-3;
    SelectPen(hDC, hPen3DLight);
    Polyline(hDC, pt, 3);

    // right vert and bottom horiz shadow
    pt[0].x = rc.right-1; pt[0].y = 1;
    pt[1].x = rc.right-1; pt[1].y = rc.bottom-1;
    pt[2].x = 0;            pt[2].y = rc.bottom-1;
    SelectPen(hDC, hPen3DDKShadow);
    Polyline(hDC, pt, 3);

    // restore the old pen
    SelectPen(hDC, hPenOld);

    DeletePen(hPen3DDKShadow);
    DeletePen(hPen3DShadow);
    DeletePen(hPen3DLight);
  }

  DrawCommandMessage(hDC, m_szCommandMessage, &rc);

  EndPaint (hWnd, &ps);
}

//**************************
// Plugin window procedure
//**************************
static LRESULT CALLBACK NP_LOADDS PluginWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
  CPlugin *pPlugin = (CPlugin *)GetWindowLong(hWnd, GWL_USERDATA);
                        
  switch(message)
  {
    case WM_CREATE:
      pPlugin = (CPlugin *)(((CREATESTRUCT FAR*)lParam)->lpCreateParams);
      assert(pPlugin != NULL);
      SetWindowLong(hWnd, GWL_USERDATA, (LONG)pPlugin);
      pPlugin->onCreate(hWnd);
      return 0L;
    case WM_LBUTTONUP:
      HANDLE_WM_LBUTTONUP(hWnd, wParam, lParam, pPlugin->onLButtonUp);
      return 0L;
    case WM_RBUTTONUP:
      HANDLE_WM_RBUTTONUP(hWnd, wParam, lParam, pPlugin->onRButtonUp);
      return 0L;
    case WM_PAINT:
      HANDLE_WM_PAINT(hWnd, wParam, lParam, pPlugin->onPaint);
      return 0L;
    case WM_MOUSEMOVE:
      dbgOut1("MouseMove");
      break;

    default:
      break;
  }
  return(DefWindowProc(hWnd, message, wParam, lParam));
}
