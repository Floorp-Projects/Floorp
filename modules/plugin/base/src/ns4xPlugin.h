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

#ifndef ns4xPlugin_h__
#define ns4xPlugin_h__

#include "nsplugin.h"
#include "prlink.h"  // for PRLibrary
#include "npupp.h"
#include "nsPluginHostImpl.h"

////////////////////////////////////////////////////////////////////////

/*
 * Use this macro before each exported function
 * (between the return address and the function
 * itself), to ensure that the function has the
 * right calling conventions on Win16.
 */

#ifdef XP_OS2
#define NP_EXPORT _System
#else
#define NP_EXPORT
#endif

////////////////////////////////////////////////////////////////////////

// XXX These are defined in platform specific FE directories right now :-/

#if defined(XP_PC) || defined(XP_UNIX) || defined(XP_BEOS)
typedef NS_CALLBACK_(NPError, NP_GETENTRYPOINTS) (NPPluginFuncs* pCallbacks);
typedef NS_CALLBACK_(NPError, NP_PLUGININIT) (const NPNetscapeFuncs* pCallbacks);
typedef NS_CALLBACK_(NPError, NP_PLUGINUNIXINIT) (const NPNetscapeFuncs* pCallbacks,NPPluginFuncs* fCallbacks);
typedef NS_CALLBACK_(NPError, NP_PLUGINSHUTDOWN) (void);
#endif

#ifdef XP_MAC
typedef NS_CALLBACK_(NPError, NP_PLUGINSHUTDOWN) (void);
typedef NS_CALLBACK_(NPError, NP_MAIN) (NPNetscapeFuncs* nCallbacks, NPPluginFuncs* pCallbacks, NPP_ShutdownUPP* unloadUpp);
#endif

class nsIServiceManagerObsolete;
class nsIMemory;

////////////////////////////////////////////////////////////////////////

/**
 * A 5.0 wrapper for a 4.x style plugin.
 */

class ns4xPlugin : public nsIPlugin
{
public:

  ns4xPlugin(NPPluginFuncs* callbacks, PRLibrary* aLibrary, NP_PLUGINSHUTDOWN aShutdown, nsIServiceManagerObsolete* serviceMgr);
  virtual ~ns4xPlugin(void);

  static void ReleaseStatics();

  NS_DECL_ISUPPORTS

  //nsIFactory interface

  NS_IMETHOD CreateInstance(nsISupports *aOuter,
                            REFNSIID aIID,
                            void **aResult);

  NS_IMETHOD LockFactory(PRBool aLock);

  //nsIPlugin interface

  /**
   * Creates a new plugin instance, based on a MIME type. This
   * allows different impelementations to be created depending on
   * the specified MIME type.
   */
  NS_IMETHOD CreatePluginInstance(nsISupports *aOuter, REFNSIID aIID, 
                                  const char* aPluginMIMEType,
                                  void **aResult);
  
  NS_IMETHOD
  Initialize(void);

  NS_IMETHOD
  Shutdown(void);

  NS_IMETHOD
  GetMIMEDescription(const char* *resultingDesc);

  NS_IMETHOD
  GetValue(nsPluginVariable variable, void *value);

  ////////////////////////////////////////////////////////////////////
  // ns4xPlugin-specific methods

  /**
   * A static factory method for constructing 4.x plugins. Constructs
   * and initializes an ns4xPlugin object, and returns it in
   * <b>result</b>.
   */
   
  static nsresult
  CreatePlugin(nsIServiceManagerObsolete* aServiceMgr,
               const char* aFileName,
               PRLibrary* aLibrary,
               nsIPlugin** aResult);

#ifdef XP_MAC
  void
  SetPluginRefNum(short aRefNum);
#endif

protected:
  /**
   * Ensures that the static CALLBACKS is properly initialized
   */
  static void CheckClassInitialized(void);


#ifdef XP_MAC
  short fPluginRefNum;
#endif

  /**
   * The plugin-side callbacks that the browser calls. One set of
   * plugin callbacks for each plugin.
   */
  NPPluginFuncs fCallbacks;
  PRLibrary*    fLibrary;

  NP_PLUGINSHUTDOWN fShutdownEntry;

  /**
   * The browser-side callbacks that a 4.x-style plugin calls.
   */
  static NPNetscapeFuncs CALLBACKS;
};

#endif // ns4xPlugin_h__
