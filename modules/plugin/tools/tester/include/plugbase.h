/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
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
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*************************************************************/
/*                                                           */
/* GUI independent part of a plugin. Base class for platform */
/* specific implementations                                  */
/*                                                           */
/*************************************************************/

#ifndef __PLUGINBASE_H__
#define __PLUGINBASE_H__

#include "xp.h"
#include "action.h"

// Prefernces profile stuff
#define NPAPI_INI_FILE_NAME   "npapi.ini"

#define DEFAULT_DWARG_VALUE   0xDDDDDDDD

class CPluginBase
{
private:
  NPP m_pNPInstance;
  WORD m_wMode;
  NPStream * m_pStream;
  NPStream * m_pScriptStream;
  char m_szScriptCacheFile[_MAX_PATH];

  // a flag to indicate that incoming stream is a special script stream
  // and needs to be treated as such. All other streams just fall through
  BOOL m_bWaitingForScriptStream;

public:
  void * m_pNPNAlloced; // used by NPN_MemFree/Alloc in manual mode
  void * m_pValue;      // used by NPN_Get/SetValue stuff

public:
  CPluginBase(NPP pNPInstance, WORD wMode);
  ~CPluginBase();

  // pure virtuals to be implemented in platform specific context
  virtual BOOL initEmbed(DWORD dwInitData) = 0;
  virtual void shutEmbed() = 0;
  virtual BOOL isInitialized() = 0; // initialized
  virtual void getModulePath(LPSTR szPath, int iSize) = 0;
  virtual int messageBox(LPSTR szMessage, LPSTR szTitle, UINT uStyle) = 0;

  // virtuals just in case
  virtual BOOL initFull(DWORD dwInitData);
  virtual void shutFull();
  virtual BOOL init(DWORD dwInitData);
  virtual void shut();
  virtual void getLogFileName(LPSTR szLogFileName, int iSize);

  const NPP getNPInstance();
  const WORD getMode();
  const NPStream * getNPStream();

  // special NPP_* call handlers
  void onNPP_NewStream(NPP pInstance, LPSTR szMIMEType, NPStream * pStream, NPBool bSeekable, uint16 * puType);
  void onNPP_StreamAsFile(NPP pInstance, NPStream * pStream, const char * szFileName);
  void onNPP_DestroyStream(NPStream * pStream);
 
  // made it virtual just in case some GUI reflection needed
  virtual DWORD makeNPNCall(NPAPI_Action = action_invalid, 
                            DWORD dw1 = 0L, DWORD dw2 = 0L, 
                            DWORD dw3 = 0L, DWORD dw4 = 0L, 
                            DWORD dw5 = 0L, DWORD dw6 = 0L, 
                            DWORD dw7 = 0L);
};

// creation and destruction of the object of the derived class
CPluginBase * CreatePlugin(NPP instance, uint16 mode);
void DestroyPlugin(CPluginBase * pPlugin);

#endif // __PLUGINBASE_H__
