/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef ns4xPlugin_h__
#define ns4xPlugin_h__

#include "nsIPlugin.h"
#include "nsIPluginInstancePeer.h"
#include "nsIWindowlessPlugInstPeer.h"
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

#if defined(XP_WIN)
#define NS_4XPLUGIN_CALLBACK(_type, _name) _type (__stdcall * _name)
#elif defined(XP_OS2)
#define NS_4XPLUGIN_CALLBACK(_type, _name) _type (_System * _name)
#else
#define NS_4XPLUGIN_CALLBACK(_type, _name) _type (* _name)
#endif

////////////////////////////////////////////////////////////////////////

// XXX These are defined in platform specific FE directories right now :-/

#if defined(XP_WIN) || defined(XP_UNIX) || defined(XP_BEOS) || defined(XP_OS2)
typedef NS_4XPLUGIN_CALLBACK(NPError, NP_GETENTRYPOINTS) (NPPluginFuncs* pCallbacks);
typedef NS_4XPLUGIN_CALLBACK(NPError, NP_PLUGININIT) (const NPNetscapeFuncs* pCallbacks);
typedef NS_4XPLUGIN_CALLBACK(NPError, NP_PLUGINUNIXINIT) (const NPNetscapeFuncs* pCallbacks,NPPluginFuncs* fCallbacks);
typedef NS_4XPLUGIN_CALLBACK(NPError, NP_PLUGINSHUTDOWN) (void);
#endif

#if defined(XP_MAC) || defined(XP_MACOSX)
typedef NS_4XPLUGIN_CALLBACK(NPError, NP_PLUGINSHUTDOWN) (void);
typedef NS_4XPLUGIN_CALLBACK(NPError, NP_MAIN) (NPNetscapeFuncs* nCallbacks, NPPluginFuncs* pCallbacks, NPP_ShutdownUPP* unloadUpp);
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
  ns4xPlugin(NPPluginFuncs* callbacks, PRLibrary* aLibrary,
             NP_PLUGINSHUTDOWN aShutdown,
             nsIServiceManagerObsolete* serviceMgr);
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
  
  NS_IMETHOD Initialize(void);

  NS_IMETHOD Shutdown(void);

  NS_IMETHOD GetMIMEDescription(const char* *resultingDesc);

  NS_IMETHOD GetValue(nsPluginVariable variable, void *value);

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
               const char* aFullPath,
               PRLibrary* aLibrary,
               nsIPlugin** aResult);

#if defined(XP_MAC) || defined(XP_MACOSX)
  void
  SetPluginRefNum(short aRefNum);
#endif

protected:
  /**
   * Ensures that the static CALLBACKS is properly initialized
   */
  static void CheckClassInitialized(void);


#if defined(XP_MAC) || defined(XP_MACOSX)
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


PR_BEGIN_EXTERN_C
NPObject* NP_EXPORT
_getwindowobject(NPP npp);

NPObject* NP_EXPORT
_getpluginelement(NPP npp);

NPIdentifier NP_EXPORT
_getstringidentifier(const NPUTF8* name);

void NP_EXPORT
_getstringidentifiers(const NPUTF8** names, int32_t nameCount,
                      NPIdentifier *identifiers);

bool NP_EXPORT
_identifierisstring(NPIdentifier identifiers);

NPIdentifier NP_EXPORT
_getintidentifier(int32_t intid);

NPUTF8* NP_EXPORT
_utf8fromidentifier(NPIdentifier identifier);

int32_t NP_EXPORT
_intfromidentifier(NPIdentifier identifier);

NPObject* NP_EXPORT
_createobject(NPP npp, NPClass* aClass);

NPObject* NP_EXPORT
_retainobject(NPObject* npobj);

void NP_EXPORT
_releaseobject(NPObject* npobj);

bool NP_EXPORT
_invoke(NPP npp, NPObject* npobj, NPIdentifier method, const NPVariant *args,
        uint32_t argCount, NPVariant *result);

bool NP_EXPORT
_invokeDefault(NPP npp, NPObject* npobj, const NPVariant *args,
               uint32_t argCount, NPVariant *result);

bool NP_EXPORT
_evaluate(NPP npp, NPObject* npobj, NPString *script, NPVariant *result);

bool NP_EXPORT
_getproperty(NPP npp, NPObject* npobj, NPIdentifier property,
             NPVariant *result);

bool NP_EXPORT
_setproperty(NPP npp, NPObject* npobj, NPIdentifier property,
             const NPVariant *value);

bool NP_EXPORT
_removeproperty(NPP npp, NPObject* npobj, NPIdentifier property);

bool NP_EXPORT
_hasproperty(NPP npp, NPObject* npobj, NPIdentifier propertyName);

bool NP_EXPORT
_hasmethod(NPP npp, NPObject* npobj, NPIdentifier methodName);

void NP_EXPORT
_releasevariantvalue(NPVariant *variant);

void NP_EXPORT
_setexception(NPObject* npobj, const NPUTF8 *message);

PR_END_EXTERN_C

const char *
PeekException();

void
PopException();

class NPPStack
{
public:
  static NPP Peek()
  {
    return sCurrentNPP;
  }

protected:
  static NPP sCurrentNPP;
};

class NPPAutoPusher : public NPPStack
{
public:
  NPPAutoPusher(NPP npp)
    : mOldNPP(sCurrentNPP)
  {
    NS_ASSERTION(npp, "Uh, null npp passed to NPPAutoPusher!");

    sCurrentNPP = npp;
  }

  ~NPPAutoPusher()
  {
    sCurrentNPP = mOldNPP;
  }

private:
  NPP mOldNPP;
};

class NPPExceptionAutoHolder
{
public:
  NPPExceptionAutoHolder();
  ~NPPExceptionAutoHolder();

protected:
  char *mOldException;
};

#endif // ns4xPlugin_h__
