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
 *   IBM Corp.
 */

#define INCL_GPI
#define INCL_WIN
#include <os2.h>
#include <string.h>

#include "npnulos2.h"

#include "plugin.h"
#include "utils.h"
#include "dialogs.h"
#include "dbg.h"

static char szNullPluginWindowClassName[] = CLASS_NULL_PLUGIN;

MRESULT EXPENTRY PluginWndProc(HWND, ULONG, MPARAM, MPARAM);

static char szDefaultPluginFinderURL[] = DEFAULT_PLUGINFINDER_URL;
static char szPageUrlForJavaScript[] = PAGE_URL_FOR_JAVASCRIPT;
static char szPageUrlForJVM[] = JVM_SMARTUPDATE_URL;
//static char szPluginFinderCommandFormatString[] = PLUGINFINDER_COMMAND;
static char szPluginFinderCommandBeginning[] = PLUGINFINDER_COMMAND_BEGINNING;
static char szPluginFinderCommandEnd[] = PLUGINFINDER_COMMAND_END;
static char szDefaultFileExt[] = "*";

BOOL RegisterNullPluginWindowClass()
{
  return WinRegisterClass( (HAB)0, szNullPluginWindowClassName, (PFNWP)PluginWndProc, 0, sizeof(ULONG));
}

void UnregisterNullPluginWindowClass()
{
}

/*********************************************/
/*                                           */
/*       CPlugin class implementation        */
/*                                           */
/*********************************************/

CPlugin::CPlugin(HMODULE hInst, 
                 NPP pNPInstance, 
                 SHORT wMode, 
                 NPMIMEType pluginType, 
                 PSZ szPageURL, 
                 PSZ szFileURL, 
                 PSZ szFileExtension,
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

  if((pluginType != NULL) && (strlen((PSZ)pluginType) != 0))
  {
    m_pNPMIMEType = (NPMIMEType)new char[strlen((PSZ)pluginType) + 1];
    if(m_pNPMIMEType != NULL)
      strcpy((PSZ)m_pNPMIMEType, pluginType);
  }

  if((szPageURL != NULL) && (strlen(szPageURL) != 0))
  {
    m_szPageURL = new char[strlen(szPageURL) + 1];
    if(m_szPageURL != NULL)
      strcpy(m_szPageURL, szPageURL);
  }
  
  if((szFileURL != NULL) && (strlen(szFileURL) != 0))
  {
    m_szFileURL = new char[strlen(szFileURL) + 1];
    if(m_szFileURL != NULL)
      strcpy(m_szFileURL, szFileURL);
  }

  if((szFileExtension != NULL) && (strlen(szFileExtension) != 0))
  {
    m_szFileExtension = new char[strlen(szFileExtension) + 1];
    if(m_szFileExtension != NULL)
      strcpy(m_szFileExtension, szFileExtension);
  }

  m_hIcon = WinLoadPointer(HWND_DESKTOP, m_hInst, IDI_PLUGICON);

  char szString[1024] = {'\0'};
  WinLoadString((HAB)0, m_hInst, IDS_CLICK_TO_GET, sizeof(szString), szString);
  if(strlen(szString) != 0)
  {
    m_szCommandMessage = new char[strlen(szString) + 1];
    if(m_szCommandMessage != NULL)
      strcpy(m_szCommandMessage, szString);
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
    WinDestroyPointer(m_hIcon);
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
    assert(WinIsWindow((HAB)0, hWndParent));

    if(WinIsWindow((HAB)0, hWndParent))
      m_hWndParent = hWndParent;

    RECTL rcParent;
    WinQueryWindowRect(m_hWndParent, &rcParent);

    m_hWnd = WinCreateWindow(m_hWndParent,
                             szNullPluginWindowClassName, 
                             "NULL Plugin", 
                             0,
                             0,0, rcParent.xRight, rcParent.yTop,
                             m_hWndParent,
                             HWND_TOP,
                             255,
                             (PVOID)this,
                             0);

    assert(m_hWnd != NULL);
    if((m_hWnd == NULL) || (!WinIsWindow((HAB)0, m_hWnd)))
      return FALSE;

//    UpdateWindow(m_hWnd);
    WinShowWindow(m_hWnd, TRUE);
  }

//  if(IsNewMimeType((PSZ)m_pNPMIMEType) || m_bHidden)
    showGetPluginDialog();

  return TRUE;
}

void CPlugin::shut()
{
  dbgOut1("CPlugin::shut()");

  if(m_hWndDialog != NULL)
  {
    WinDismissDlg(m_hWndDialog, DID_CANCEL);
    m_hWndDialog = NULL;
  }

  if(m_hWnd != NULL)
  {
    WinDestroyWindow(m_hWnd);
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

PSZ CPlugin::createURLString()
{
  if(m_szURLString != NULL)
  {
    delete [] m_szURLString;
    m_szURLString = NULL;
  }

  // check if there is file URL first
  if(m_szFileURL != NULL)
  {
    m_szURLString = new char[strlen(m_szFileURL) + 1];
    if(m_szURLString == NULL)
      return NULL;
    strcpy(m_szURLString, m_szFileURL);
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
    szAddress = new char[strlen(m_szPageURL) + 1];
    if(szAddress == NULL)
      return NULL;
    strcpy(szAddress, m_szPageURL);

    m_szURLString = new char[strlen(szAddress) + 1 + strlen((PSZ)m_pNPMIMEType) + 1];

    if(m_szURLString == NULL)
      return NULL;

    // Append the MIME type to the URL
    sprintf(m_szURLString, "%s?%s", szAddress, (PSZ)m_pNPMIMEType);
  }
  else // default
  {
    if(!m_bJavaScript)
    {
      urlToOpen = szDefaultPluginFinderURL;
      
      if (contentTypeIsJava) {
        urlToOpen = szPageUrlForJVM;
      }

      szAddress = new char[strlen(urlToOpen) + 1];
      if(szAddress == NULL)
        return NULL;
      strcpy(szAddress, urlToOpen);

      m_szURLString = new char[strlen(szAddress) + 1 + strlen((PSZ)m_pNPMIMEType) + 1];

      if(m_szURLString == NULL)
        return NULL;

      // Append the MIME type to the URL
      sprintf(m_szURLString, "%s?%s", szAddress, (PSZ)m_pNPMIMEType);
    }
    else
    {
      urlToOpen = szPageUrlForJavaScript;

      if (contentTypeIsJava) {
	urlToOpen = szPageUrlForJVM;
      }

      m_szURLString = new char[strlen(szPluginFinderCommandBeginning) + strlen(urlToOpen) + 10 + 
                               strlen((PSZ)m_pNPMIMEType) + strlen(szPluginFinderCommandEnd) + 1];
      sprintf(m_szURLString, "%s%s?mimetype=%s%s",
               szPluginFinderCommandBeginning, urlToOpen, 
               (PSZ)m_pNPMIMEType, szPluginFinderCommandEnd);
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

  sprintf(szJSString, 
           szPluginFinderCommandFormatString, 
           szDefaultPluginFinderURL, 
           m_pNPMIMEType, 
           (m_szFileExtension != NULL) ? m_szFileExtension : szDefaultFileExt);

  dbgOut3("%#08x '%s'", m_pNPInstance, szJSString);

  assert(strlen(szJSString) > 0);

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
    int iRet = WinDlgBox(HWND_DESKTOP, m_hWnd, (PFNWP)GetPluginDialogProc, m_hInst,
                                  IDD_PLUGIN_DOWNLOAD, (PVOID)this);
    if(iRet != IDC_GET_PLUGIN)
      return;
  }

  if(m_szCommandMessage != NULL)
  {
    delete [] m_szCommandMessage;
    m_szCommandMessage = NULL;
  }

  char szString[1024] = {'\0'};
  WinLoadString((HAB)0, m_hInst, IDS_CLICK_WHEN_DONE, sizeof(szString), szString);
  if(strlen(szString) != 0)
  {
    m_szCommandMessage = new char[strlen(szString) + 1];
    if(m_szCommandMessage != NULL)
      strcpy(m_szCommandMessage, szString);
  }

  WinInvalidateRect(m_hWnd, NULL, TRUE);
//  UpdateWindow(m_hWnd);

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
  
  int iSize = WinLoadString((HAB)0, m_hInst, IDS_GOING2HTML, sizeof(buf), buf);

  NPError rc = NPN_NewStream(m_pNPInstance, "text/html", "asd_plugin_finder", &pStream);

  //char buf[] = "<html>\n<body>\n\n<h2 align=center>NPN_NewStream / NPN_Write - This seems to work.</h2>\n\n</body>\n</html>";
  
  int32 iBytes = NPN_Write(m_pNPInstance, pStream, strlen(buf), buf);

  NPN_DestroyStream(m_pNPInstance, pStream, NPRES_DONE);
}

BOOL CPlugin::readyToRefresh()
{
  char szString[1024] = {'\0'};
  WinLoadString((HAB)0, m_hInst, IDS_CLICK_WHEN_DONE, sizeof(szString), szString);
  if(m_szCommandMessage == NULL)
    return FALSE;

  return (strcmp(m_szCommandMessage, szString) == 0);
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

static void DrawCommandMessage(HPS hPS, PSZ szString, PRECTL lprc)
{
  if(szString == NULL)
    return;

  POINTL ptls[5];
  GpiQueryTextBox(hPS, strlen(szString), szString, 5, ptls);
  POINTL pt;
  pt.x = ptls[TXTBOX_CONCAT].x;
  pt.y = ptls[TXTBOX_TOPLEFT].y - ptls[TXTBOX_BOTTOMLEFT].y;

  int iY = (lprc->yTop / 2) - ((32) / 2) + 36;
  int iX = 0;

  if(lprc->xRight > pt.x)
    iX = lprc->xRight/2 - pt.x/2;
  else
    iX = 1;

  RECTL rcText;
  rcText.xLeft   = iX;
  rcText.xRight  = rcText.xLeft + pt.x;
  rcText.yBottom    = iY;
  rcText.yTop = rcText.yBottom + pt.y;

  WinDrawText(hPS, strlen(szString), szString, &rcText, 0, 0,
              DT_TEXTATTRS | DT_CENTER | DT_VCENTER);
}

void CPlugin::onPaint(HWND hWnd)
{
  RECTL rc;
  HDC hPS;
  int x, y;

  hPS = WinBeginPaint(hWnd, NULLHANDLE, NULL);
  GpiErase(hPS);
  WinQueryWindowRect(hWnd, &rc);

  x = (rc.xRight / 2) - (32 / 2);
  y = (rc.yTop / 2) - ((32) / 2);

  if(rc.xRight > 34 && rc.yTop > 34)
  {
    if(m_hIcon != NULL)
      WinDrawPointer(hPS, x, y, m_hIcon, DP_NORMAL);

    POINTL pt[5];

    // left vert and top horiz highlight
    pt[0].x = 1;            pt[0].y = rc.yTop-1;
    pt[1].x = 1;            pt[1].y = 1;
    pt[2].x = rc.xRight-1; pt[2].y = 1;

    GpiPolyLine(hPS, 3, pt);

    // left vert and top horiz shadow
    pt[0].x = 2;            pt[0].y = rc.yTop-3;
    pt[1].x = 2;            pt[1].y = 2;
    pt[2].x = rc.xRight-2; pt[2].y = 2;
    GpiPolyLine(hPS, 3, pt);

    // right vert and bottom horiz highlight
    pt[0].x = rc.xRight-3; pt[0].y = 2;
    pt[1].x = rc.xRight-3; pt[1].y = rc.yTop-3;
    pt[2].x = 2;            pt[2].y = rc.yTop-3;
    GpiPolyLine(hPS, 3, pt);

    // right vert and bottom horiz shadow
    pt[0].x = rc.xRight-1; pt[0].y = 1;
    pt[1].x = rc.xRight-1; pt[1].y = rc.yTop-1;
    pt[2].x = 0;            pt[2].y = rc.yTop-1;
    GpiPolyLine(hPS, 3, pt);
  }

  DrawCommandMessage(hPS, m_szCommandMessage, &rc);

  WinEndPaint (hPS);
}

//**************************
// Plugin window procedure
//**************************
MRESULT EXPENTRY NP_LOADDS PluginWndProc(HWND hWnd, ULONG message, MPARAM mp1, MPARAM mp2)
{
  CPlugin *pPlugin = (CPlugin *)WinQueryWindowULong(hWnd, QWL_USER);
                        
  switch(message)
  {
    case WM_CREATE:
      pPlugin = (CPlugin *)(((PCREATESTRUCT)mp2)->pCtlData);
      assert(pPlugin != NULL);
      WinSetWindowULong(hWnd, QWL_USER, (ULONG)pPlugin);
      pPlugin->onCreate(hWnd);
      return 0L;
    case WM_BUTTON1UP:
      pPlugin->onLButtonUp(hWnd,0,0,0);
      return 0L;
   case WM_BUTTON2UP:
      pPlugin->onRButtonUp(hWnd,0,0,0);
      return 0L;
    case WM_PAINT:
      pPlugin->onPaint(hWnd);
      return 0L;
    case WM_MOUSEMOVE:
      dbgOut1("MouseMove");
      break;

    default:
      break;
  }
  return(WinDefWindowProc(hWnd, message, mp1, mp2));
}
