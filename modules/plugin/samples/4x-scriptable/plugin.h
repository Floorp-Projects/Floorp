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
 */

#ifndef __PLUGIN_H__
#define __PLUGIN_H__

#include "npapi.h"
#include "nsScriptablePeer.h"

class CPlugin
{
private:
  NPP m_pNPInstance;

#ifdef XP_WIN
  HWND m_hWnd; 
#endif

  NPStream * m_pNPStream;
  NPBool m_bInitialized;
  nsI4xScriptablePlugin * m_pScriptablePeer;

public:
  char m_String[128];

public:
  CPlugin(NPP pNPInstance);
  ~CPlugin();

  NPBool init(NPWindow* pNPWindow);
  void shut();
  NPBool isInitialized();

  void showVersion();
  void clear();
  void getVersion(char* *aVersion);

  nsI4xScriptablePlugin* getScriptablePeer();
};

#endif // __PLUGIN_H__
