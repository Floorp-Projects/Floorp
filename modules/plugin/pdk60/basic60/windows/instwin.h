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

#ifndef __INSTWIN_H__
#define __INSTWIN_H__

#include "instbase.h"

class CPlatformWin
{
public:
  nsresult init(nsPluginWindow * window);
  void shut();

  CPlatformWin();
  ~CPlatformWin();

  HWND m_hWnd;
  WNDPROC m_OldWindowProc;
  PRBool m_bInitialized;
};

class CPluginInstanceWin : public CPluginInstance
{
  nsresult PlatformNew();
  nsresult PlatformDestroy();
  nsresult PlatformSetWindow(nsPluginWindow* window);
  PRInt16 PlatformHandleEvent(nsPluginEvent* event);

public:
  CPluginInstanceWin();
  ~CPluginInstanceWin();

  CPlatformWin * platform;
};

#endif //__INSTWIN_H__
