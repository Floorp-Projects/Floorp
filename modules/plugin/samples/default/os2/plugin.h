/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications Corporation.
 * Portions created by Netscape Communications Corporation are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the LGPL or the GPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */

#ifndef __PLUGIN_HPP__
#define __PLUGIN_HPP__

#include "npapi.h"

class CPlugin
{
private:
  HMODULE m_hInst;
  NPP m_pNPInstance;
  SHORT m_wMode;
  HWND m_hWnd;
  HWND m_hWndParent;
  HPOINTER m_hIcon;
  char * m_szURLString;

  char * m_szCommandMessage;

public:
  BOOL m_bHidden;
  NPMIMEType m_pNPMIMEType;
  PSZ m_szPageURL;       // Location of plug-in HTML page
  PSZ m_szFileURL;       // Location of plug-in JAR file 
  PSZ m_szFileExtension; // File extension associated with the of the unknown mimetype
  HWND m_hWndDialog;

  // environment
  BOOL m_bOnline;
  BOOL m_bJava;
  BOOL m_bJavaScript;
  BOOL m_bSmartUpdate;

private:
  BOOL useDefaultURL_P();
  BOOL doSmartUpdate_P();
  PSZ createURLString();
  void getPluginSmart();
  void getPluginRegular();

public:
  CPlugin(HMODULE hInst, 
          NPP pNPInstance, 
          SHORT wMode, 
          NPMIMEType pluginType, 
          PSZ szPageURL, 
          PSZ szFileURL, 
          PSZ szFileExtension,
          BOOL bHidden);
  ~CPlugin();

  BOOL init(HWND hWnd);
  void shut();
  HWND getWindow();
  void showGetPluginDialog();
  BOOL readyToRefresh();

  // NP API handlers
  void resize();
  void print(NPPrint * pNPPrint);
  void URLNotify(const char * szURL);

  // Windows message handlers
  void onCreate(HWND hWnd);
  void onLButtonUp(HWND hWnd, int x, int y, UINT keyFlags);
  void onRButtonUp(HWND hWnd, int x, int y, UINT keyFlags);
  void onPaint(HWND hWnd);
};


#define PAGE_URL_FOR_JAVASCRIPT "http://cgi.netscape.com/cgi-bin/plugins/get_plugin.cgi"

#define PLUGINFINDER_COMMAND_BEGINNING "javascript:window.open(\""
#define PLUGINFINDER_COMMAND_END "\",\"plugin\",\"toolbar=no,status=no,resizeable=no,scrollbars=no,height=252,width=626\");"
#define DEFAULT_PLUGINFINDER_URL "http://cgi.netscape.com/cgi-bin/plug-in_finder.cgi"
#define JVM_SMARTUPDATE_URL "http://home.netscape.com/plugins/jvm.html"

#ifdef WIN32
#define REGISTRY_PLACE "Software\\Netscape\\Netscape Navigator\\Default Plugin"
#else
#define GWL_USERDATA        0
#define COLOR_3DSHADOW      COLOR_BTNFACE
#define COLOR_3DLIGHT       COLOR_BTNHIGHLIGHT
#define COLOR_3DDKSHADOW    COLOR_BTNSHADOW
#endif

#define CLASS_NULL_PLUGIN "NullPluginClass"
 
BOOL RegisterNullPluginWindowClass();
void UnregisterNullPluginWindowClass();

extern HMODULE hInst;

#endif // __PLUGIN_HPP__
