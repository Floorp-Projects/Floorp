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

class nsIServiceManager;
class nsIMemory;

////////////////////////////////////////////////////////////////////////

/**
 * A 5.0 wrapper for a 4.x style plugin.
 */

class ns4xPlugin : public nsIPlugin
{
public:

  ns4xPlugin(NPPluginFuncs* callbacks, PRLibrary* aLibrary, NP_PLUGINSHUTDOWN aShutdown, nsIServiceManager* serviceMgr);
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
  CreatePlugin(nsIServiceManager* aServiceMgr,
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

XP_BEGIN_PROTOS

  ////////////////////////////////////////////////////////////////////////
  // Static stub functions that are exported to the 4.x plugin as entry
  // points via the CALLBACKS variable.
  //
  static NPError NP_EXPORT
  _requestread(NPStream *pstream, NPByteRange *rangeList);

  static NPError NP_EXPORT
  _geturlnotify(NPP npp, const char* relativeURL, const char* target, void* notifyData);

  static NPError NP_EXPORT
  _getvalue(NPP npp, NPNVariable variable, void *r_value);

  static NPError NP_EXPORT
  _setvalue(NPP npp, NPPVariable variable, void *r_value);

  static NPError NP_EXPORT
  _geturl(NPP npp, const char* relativeURL, const char* target);

  static NPError NP_EXPORT
  _posturlnotify(NPP npp, const char* relativeURL, const char *target,
                    uint32 len, const char *buf, NPBool file, void* notifyData);

  static NPError NP_EXPORT
  _posturl(NPP npp, const char* relativeURL, const char *target, uint32 len,
              const char *buf, NPBool file);

  static NPError NP_EXPORT
  _newstream(NPP npp, NPMIMEType type, const char* window, NPStream** pstream);

  static int32 NP_EXPORT
  _write(NPP npp, NPStream *pstream, int32 len, void *buffer);

  static NPError NP_EXPORT
  _destroystream(NPP npp, NPStream *pstream, NPError reason);

  static void NP_EXPORT
  _status(NPP npp, const char *message);

#if 0

  static void NP_EXPORT
  _registerwindow(NPP npp, void* window);

  static void NP_EXPORT
  _unregisterwindow(NPP npp, void* window);

  static int16 NP_EXPORT
  _allocateMenuID(NPP npp, NPBool isSubmenu);

#endif

  static void NP_EXPORT
  _memfree (void *ptr);

  static uint32 NP_EXPORT
  _memflush(uint32 size);

  static void NP_EXPORT
  _reloadplugins(NPBool reloadPages);

  static void NP_EXPORT
  _invalidaterect(NPP npp, NPRect *invalidRect);

  static void NP_EXPORT
  _invalidateregion(NPP npp, NPRegion invalidRegion);

  static void NP_EXPORT
  _forceredraw(NPP npp);

  ////////////////////////////////////////////////////////////////////////
  // Anything that returns a pointer needs to be _HERE_ for 68K Mac to
  // work.
  //

#if defined(XP_MAC) && !defined(powerc)
#pragma pointers_in_D0
#endif

  static const char* NP_EXPORT
  _useragent(NPP npp);

  static void* NP_EXPORT
  _memalloc (uint32 size);

  static JRIEnv* NP_EXPORT
  _getJavaEnv(void);

#if 1

  static jref NP_EXPORT
  _getJavaPeer(NPP npp);

  static java_lang_Class* NP_EXPORT
  _getJavaClass(void* handle);

#endif

#if defined(XP_MAC) && !defined(powerc)
#pragma pointers_in_A0
#endif

XP_END_PROTOS

#endif // ns4xPlugin_h__
