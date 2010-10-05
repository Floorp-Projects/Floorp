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
 *   Tim Copperfield <timecop@network.email.ne.jp>
 *   Roland Mainz <roland.mainz@informatik.med.uni-giessen.de>
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

#include "prlog.h"
#include "prmem.h"
#include "nscore.h"
#include "prenv.h"

#include "nsNPAPIPluginInstance.h"
#include "nsNPAPIPlugin.h"
#include "nsNPAPIPluginStreamListener.h"
#include "nsPluginHost.h"
#include "nsPluginSafety.h"
#include "nsPluginLogging.h"
#include "nsIPrivateBrowsingService.h"

#include "nsIDocument.h"
#include "nsIScriptGlobalObject.h"
#include "nsIScriptContext.h"
#include "nsDirectoryServiceDefs.h"

#include "nsJSNPRuntime.h"

using namespace mozilla::plugins::parent;
using mozilla::TimeStamp;

static NS_DEFINE_IID(kIOutputStreamIID, NS_IOUTPUTSTREAM_IID);
static NS_DEFINE_IID(kIPluginStreamListenerIID, NS_IPLUGINSTREAMLISTENER_IID);

NS_IMPL_ISUPPORTS1(nsNPAPIPluginInstance, nsIPluginInstance)

nsNPAPIPluginInstance::nsNPAPIPluginInstance(nsNPAPIPlugin* plugin)
  :
#ifdef XP_MACOSX
#ifdef NP_NO_QUICKDRAW
    mDrawingModel(NPDrawingModelCoreGraphics),
#else
    mDrawingModel(NPDrawingModelQuickDraw),
#endif
#endif
    mRunning(NOT_STARTED),
    mWindowless(PR_FALSE),
    mWindowlessLocal(PR_FALSE),
    mTransparent(PR_FALSE),
    mCached(PR_FALSE),
    mWantsAllNetworkStreams(PR_FALSE),
    mInPluginInitCall(PR_FALSE),
    mPlugin(plugin),
    mMIMEType(nsnull),
    mOwner(nsnull),
    mCurrentPluginEvent(nsnull),
#ifdef MOZ_X11
    mUsePluginLayersPref(PR_TRUE)
#else
    mUsePluginLayersPref(PR_FALSE)
#endif
{
  NS_ASSERTION(mPlugin != NULL, "Plugin is required when creating an instance.");

  // Initialize the NPP structure.

  mNPP.pdata = NULL;
  mNPP.ndata = this;

  nsCOMPtr<nsIPrefBranch> prefs(do_GetService(NS_PREFSERVICE_CONTRACTID));
  if (prefs) {
    PRBool useLayersPref;
    nsresult rv = prefs->GetBoolPref("mozilla.plugins.use_layers", &useLayersPref);
    if (NS_SUCCEEDED(rv))
      mUsePluginLayersPref = useLayersPref;
  }

  PLUGIN_LOG(PLUGIN_LOG_BASIC, ("nsNPAPIPluginInstance ctor: this=%p\n",this));
}

nsNPAPIPluginInstance::~nsNPAPIPluginInstance()
{
  PLUGIN_LOG(PLUGIN_LOG_BASIC, ("nsNPAPIPluginInstance dtor: this=%p\n",this));

  if (mMIMEType) {
    PR_Free((void *)mMIMEType);
    mMIMEType = nsnull;
  }
}

void
nsNPAPIPluginInstance::Destroy()
{
  Stop();
  mPlugin = nsnull;
}

TimeStamp
nsNPAPIPluginInstance::LastStopTime()
{
  return mStopTime;
}

NS_IMETHODIMP nsNPAPIPluginInstance::Initialize(nsIPluginInstanceOwner* aOwner, const char* aMIMEType)
{
  PLUGIN_LOG(PLUGIN_LOG_NORMAL, ("nsNPAPIPluginInstance::Initialize this=%p\n",this));

  mOwner = aOwner;

  if (aMIMEType) {
    mMIMEType = (char*)PR_Malloc(PL_strlen(aMIMEType) + 1);

    if (mMIMEType)
      PL_strcpy(mMIMEType, aMIMEType);
  }

  return InitializePlugin();
}

NS_IMETHODIMP nsNPAPIPluginInstance::Start()
{
  PLUGIN_LOG(PLUGIN_LOG_NORMAL, ("nsNPAPIPluginInstance::Start this=%p\n",this));

  if (RUNNING == mRunning)
    return NS_OK;

  return InitializePlugin();
}

NS_IMETHODIMP nsNPAPIPluginInstance::Stop()
{
  PLUGIN_LOG(PLUGIN_LOG_NORMAL, ("nsNPAPIPluginInstance::Stop this=%p\n",this));

  // Make sure the plugin didn't leave popups enabled.
  if (mPopupStates.Length() > 0) {
    nsCOMPtr<nsPIDOMWindow> window = GetDOMWindow();

    if (window) {
      window->PopPopupControlState(openAbused);
    }
  }

  if (RUNNING != mRunning) {
    return NS_OK;
  }

  // clean up all outstanding timers
  for (PRUint32 i = mTimers.Length(); i > 0; i--)
    UnscheduleTimer(mTimers[i - 1]->id);

  // If there's code from this plugin instance on the stack, delay the
  // destroy.
  if (PluginDestructionGuard::DelayDestroy(this)) {
    return NS_OK;
  }

  // Make sure we lock while we're writing to mRunning after we've
  // started as other threads might be checking that inside a lock.
  EnterAsyncPluginThreadCallLock();
  mRunning = DESTROYING;
  mStopTime = TimeStamp::Now();
  ExitAsyncPluginThreadCallLock();

  OnPluginDestroy(&mNPP);

  // clean up open streams
  while (mPStreamListeners.Length() > 0) {
    nsRefPtr<nsNPAPIPluginStreamListener> currentListener(mPStreamListeners[0]);
    currentListener->CleanUpStream(NPRES_USER_BREAK);
    mPStreamListeners.RemoveElement(currentListener);
  }

  if (!mPlugin)
    return NS_ERROR_FAILURE;

  PluginLibrary* library = mPlugin->GetLibrary();
  if (!library)
    return NS_ERROR_FAILURE;

  NPPluginFuncs* pluginFunctions = mPlugin->PluginFuncs();

  NPError error = NPERR_GENERIC_ERROR;
  if (pluginFunctions->destroy) {
    NPSavedData *sdata = 0;

    NS_TRY_SAFE_CALL_RETURN(error, (*pluginFunctions->destroy)(&mNPP, &sdata), library, this);

    NPP_PLUGIN_LOG(PLUGIN_LOG_NORMAL,
                   ("NPP Destroy called: this=%p, npp=%p, return=%d\n", this, &mNPP, error));
  }
  mRunning = DESTROYED;

  nsJSNPRuntime::OnPluginDestroy(&mNPP);

  if (error != NPERR_NO_ERROR)
    return NS_ERROR_FAILURE;
  else
    return NS_OK;
}

already_AddRefed<nsPIDOMWindow>
nsNPAPIPluginInstance::GetDOMWindow()
{
  nsCOMPtr<nsIPluginInstanceOwner> owner;
  GetOwner(getter_AddRefs(owner));
  if (!owner)
    return nsnull;

  nsCOMPtr<nsIDocument> doc;
  owner->GetDocument(getter_AddRefs(doc));
  if (!doc)
    return nsnull;

  nsPIDOMWindow *window = doc->GetWindow();
  NS_IF_ADDREF(window);

  return window;
}

nsresult
nsNPAPIPluginInstance::GetTagType(nsPluginTagType *result)
{
  if (mOwner) {
    nsCOMPtr<nsIPluginTagInfo> tinfo(do_QueryInterface(mOwner));
    if (tinfo)
      return tinfo->GetTagType(result);
  }

  return NS_ERROR_FAILURE;
}

nsresult
nsNPAPIPluginInstance::GetAttributes(PRUint16& n, const char*const*& names,
                                     const char*const*& values)
{
  if (mOwner) {
    nsCOMPtr<nsIPluginTagInfo> tinfo(do_QueryInterface(mOwner));
    if (tinfo)
      return tinfo->GetAttributes(n, names, values);
  }

  return NS_ERROR_FAILURE;
}

nsresult
nsNPAPIPluginInstance::GetParameters(PRUint16& n, const char*const*& names,
                                     const char*const*& values)
{
  if (mOwner) {
    nsCOMPtr<nsIPluginTagInfo> tinfo(do_QueryInterface(mOwner));
    if (tinfo)
      return tinfo->GetParameters(n, names, values);
  }

  return NS_ERROR_FAILURE;
}

nsresult
nsNPAPIPluginInstance::GetMode(PRInt32 *result)
{
  if (mOwner)
    return mOwner->GetMode(result);
  else
    return NS_ERROR_FAILURE;
}

nsTArray<nsNPAPIPluginStreamListener*>*
nsNPAPIPluginInstance::PStreamListeners()
{
  return &mPStreamListeners;
}

nsTArray<nsPluginStreamListenerPeer*>*
nsNPAPIPluginInstance::BStreamListeners()
{
  return &mBStreamListeners;
}

nsresult
nsNPAPIPluginInstance::InitializePlugin()
{ 
  PluginDestructionGuard guard(this);

  PRUint16 count = 0;
  const char* const* names = nsnull;
  const char* const* values = nsnull;
  nsPluginTagType tagtype;
  nsresult rv = GetTagType(&tagtype);
  if (NS_SUCCEEDED(rv)) {
    // Note: If we failed to get the tag type, we may be a full page plugin, so no arguments
    rv = GetAttributes(count, names, values);
    NS_ENSURE_SUCCESS(rv, rv);
    
    // nsPluginTagType_Object or Applet may also have PARAM tags
    // Note: The arrays handed back by GetParameters() are
    // crafted specially to be directly behind the arrays from GetAttributes()
    // with a null entry as a separator. This is for 4.x backwards compatibility!
    // see bug 111008 for details
    if (tagtype != nsPluginTagType_Embed) {
      PRUint16 pcount = 0;
      const char* const* pnames = nsnull;
      const char* const* pvalues = nsnull;    
      if (NS_SUCCEEDED(GetParameters(pcount, pnames, pvalues))) {
        NS_ASSERTION(!values[count], "attribute/parameter array not setup correctly for NPAPI plugins");
        if (pcount)
          count += ++pcount; // if it's all setup correctly, then all we need is to
                             // change the count (attrs + PARAM/blank + params)
      }
    }
  }

  PRInt32       mode;
  const char*   mimetype;
  NPError       error = NPERR_GENERIC_ERROR;

  GetMode(&mode);
  GetMIMEType(&mimetype);

  // Some older versions of Flash have a bug in them
  // that causes the stack to become currupt if we
  // pass swliveconnect=1 in the NPP_NewProc arrays.
  // See bug 149336 (UNIX), bug 186287 (Mac)
  //
  // The code below disables the attribute unless
  // the environment variable:
  // MOZILLA_PLUGIN_DISABLE_FLASH_SWLIVECONNECT_HACK
  // is set.
  //
  // It is okay to disable this attribute because
  // back in 4.x, scripting required liveconnect to
  // start Java which was slow. Scripting no longer
  // requires starting Java and is quick plus controled
  // from the browser, so Flash now ignores this attribute.
  //
  // This code can not be put at the time of creating
  // the array because we may need to examine the
  // stream header to determine we want Flash.

  static const char flashMimeType[] = "application/x-shockwave-flash";
  static const char blockedParam[] = "swliveconnect";
  if (count && !PL_strcasecmp(mimetype, flashMimeType)) {
    static int cachedDisableHack = 0;
    if (!cachedDisableHack) {
       if (PR_GetEnv("MOZILLA_PLUGIN_DISABLE_FLASH_SWLIVECONNECT_HACK"))
         cachedDisableHack = -1;
       else
         cachedDisableHack = 1;
    }
    if (cachedDisableHack > 0) {
      for (PRUint16 i=0; i<count; i++) {
        if (!PL_strcasecmp(names[i], blockedParam)) {
          // BIG FAT WARNIG:
          // I'm ugly casting |const char*| to |char*| and altering it
          // because I know we do malloc it values in
          // http://bonsai.mozilla.org/cvsblame.cgi?file=mozilla/layout/html/base/src/nsObjectFrame.cpp&rev=1.349&root=/cvsroot#3020
          // and free it at line #2096, so it couldn't be a const ptr to string literal
          char *val = (char*) values[i];
          if (val && *val) {
            // we cannot just *val=0, it won't be free properly in such case
            val[0] = '0';
            val[1] = 0;
          }
          break;
        }
      }
    }
  }

  PRBool oldVal = mInPluginInitCall;
  mInPluginInitCall = PR_TRUE;

  // Need this on the stack before calling NPP_New otherwise some callbacks that
  // the plugin may make could fail (NPN_HasProperty, for example).
  NPPAutoPusher autopush(&mNPP);

  if (!mPlugin)
    return NS_ERROR_FAILURE;

  PluginLibrary* library = mPlugin->GetLibrary();
  if (!library)
    return NS_ERROR_FAILURE;

  // Mark this instance as running before calling NPP_New because the plugin may
  // call other NPAPI functions, like NPN_GetURLNotify, that assume this is set
  // before returning. If the plugin returns failure, we'll clear it out below.
  mRunning = RUNNING;

  nsresult newResult = library->NPP_New((char*)mimetype, &mNPP, (PRUint16)mode, count, (char**)names, (char**)values, NULL, &error);
  if (NS_FAILED(newResult)) {
    mRunning = DESTROYED;
    return newResult;
  }

  mInPluginInitCall = oldVal;

  NPP_PLUGIN_LOG(PLUGIN_LOG_NORMAL,
  ("NPP New called: this=%p, npp=%p, mime=%s, mode=%d, argc=%d, return=%d\n",
  this, &mNPP, mimetype, mode, count, error));

  if (error != NPERR_NO_ERROR) {
    mRunning = DESTROYED;
    return NS_ERROR_FAILURE;
  }
  
  return NS_OK;
}

NS_IMETHODIMP nsNPAPIPluginInstance::SetWindow(NPWindow* window)
{
  // NPAPI plugins don't want a SetWindow(NULL).
  if (!window || RUNNING != mRunning)
    return NS_OK;

#if defined(MOZ_WIDGET_GTK2)
  // bug 108347, flash plugin on linux doesn't like window->width <=
  // 0, but Java needs wants this call.
  if (!nsPluginHost::IsJavaMIMEType(mMIMEType) && window->type == NPWindowTypeWindow &&
      (window->width <= 0 || window->height <= 0)) {
    return NS_OK;
  }
#endif

  if (!mPlugin)
    return NS_ERROR_FAILURE;

  PluginLibrary* library = mPlugin->GetLibrary();
  if (!library)
    return NS_ERROR_FAILURE;

  NPPluginFuncs* pluginFunctions = mPlugin->PluginFuncs();

  if (pluginFunctions->setwindow) {
    PluginDestructionGuard guard(this);

    // XXX Turns out that NPPluginWindow and NPWindow are structurally
    // identical (on purpose!), so there's no need to make a copy.

    PLUGIN_LOG(PLUGIN_LOG_NORMAL, ("nsNPAPIPluginInstance::SetWindow (about to call it) this=%p\n",this));

    PRBool oldVal = mInPluginInitCall;
    mInPluginInitCall = PR_TRUE;

    NPPAutoPusher nppPusher(&mNPP);

    NPError error;
    NS_TRY_SAFE_CALL_RETURN(error, (*pluginFunctions->setwindow)(&mNPP, (NPWindow*)window), library, this);

    mInPluginInitCall = oldVal;

    NPP_PLUGIN_LOG(PLUGIN_LOG_NORMAL,
    ("NPP SetWindow called: this=%p, [x=%d,y=%d,w=%d,h=%d], clip[t=%d,b=%d,l=%d,r=%d], return=%d\n",
    this, window->x, window->y, window->width, window->height,
    window->clipRect.top, window->clipRect.bottom, window->clipRect.left, window->clipRect.right, error));
  }
  return NS_OK;
}

/* NOTE: the caller must free the stream listener */
// Create a normal stream, one without a urlnotify callback
NS_IMETHODIMP
nsNPAPIPluginInstance::NewStreamToPlugin(nsIPluginStreamListener** listener)
{
  return NewNotifyStream(listener, nsnull, PR_FALSE, nsnull);
}

NS_IMETHODIMP
nsNPAPIPluginInstance::NewStreamFromPlugin(const char* type, const char* target,
                                           nsIOutputStream* *result)
{
  nsPluginStreamToFile* stream = new nsPluginStreamToFile(target, mOwner);
  if (!stream)
    return NS_ERROR_OUT_OF_MEMORY;

  return stream->QueryInterface(kIOutputStreamIID, (void**)result);
}

// Create a stream that will notify when complete
nsresult nsNPAPIPluginInstance::NewNotifyStream(nsIPluginStreamListener** listener, 
                                                void* notifyData,
                                                PRBool aCallNotify,
                                                const char* aURL)
{
  nsNPAPIPluginStreamListener* stream = new nsNPAPIPluginStreamListener(this, notifyData, aURL);
  NS_ENSURE_TRUE(stream, NS_ERROR_OUT_OF_MEMORY);

  mPStreamListeners.AppendElement(stream);
  stream->SetCallNotify(aCallNotify); // set flag in stream to call URLNotify

  return stream->QueryInterface(kIPluginStreamListenerIID, (void**)listener);
}

NS_IMETHODIMP nsNPAPIPluginInstance::Print(NPPrint* platformPrint)
{
  NS_ENSURE_TRUE(platformPrint, NS_ERROR_NULL_POINTER);

  PluginDestructionGuard guard(this);

  if (!mPlugin)
    return NS_ERROR_FAILURE;

  PluginLibrary* library = mPlugin->GetLibrary();
  if (!library)
    return NS_ERROR_FAILURE;

  NPPluginFuncs* pluginFunctions = mPlugin->PluginFuncs();

  NPPrint* thePrint = (NPPrint *)platformPrint;

  // to be compatible with the older SDK versions and to match what
  // NPAPI and other browsers do, overwrite |window.type| field with one
  // more copy of |platformPrint|. See bug 113264
  PRUint16 sdkmajorversion = (pluginFunctions->version & 0xff00)>>8;
  PRUint16 sdkminorversion = pluginFunctions->version & 0x00ff;
  if ((sdkmajorversion == 0) && (sdkminorversion < 11)) {
    // Let's copy platformPrint bytes over to where it was supposed to be
    // in older versions -- four bytes towards the beginning of the struct
    // but we should be careful about possible misalignments
    if (sizeof(NPWindowType) >= sizeof(void *)) {
      void* source = thePrint->print.embedPrint.platformPrint;
      void** destination = (void **)&(thePrint->print.embedPrint.window.type);
      *destination = source;
    } else {
      NS_ERROR("Incompatible OS for assignment");
    }
  }

  if (pluginFunctions->print)
      NS_TRY_SAFE_CALL_VOID((*pluginFunctions->print)(&mNPP, thePrint), library, this);

  NPP_PLUGIN_LOG(PLUGIN_LOG_NORMAL,
  ("NPP PrintProc called: this=%p, pDC=%p, [x=%d,y=%d,w=%d,h=%d], clip[t=%d,b=%d,l=%d,r=%d]\n",
  this,
  platformPrint->print.embedPrint.platformPrint,
  platformPrint->print.embedPrint.window.x,
  platformPrint->print.embedPrint.window.y,
  platformPrint->print.embedPrint.window.width,
  platformPrint->print.embedPrint.window.height,
  platformPrint->print.embedPrint.window.clipRect.top,
  platformPrint->print.embedPrint.window.clipRect.bottom,
  platformPrint->print.embedPrint.window.clipRect.left,
  platformPrint->print.embedPrint.window.clipRect.right));

  return NS_OK;
}

NS_IMETHODIMP nsNPAPIPluginInstance::HandleEvent(void* event, PRInt16* result)
{
  if (RUNNING != mRunning)
    return NS_OK;

  if (!event)
    return NS_ERROR_FAILURE;

  PluginDestructionGuard guard(this);

  if (!mPlugin)
    return NS_ERROR_FAILURE;

  PluginLibrary* library = mPlugin->GetLibrary();
  if (!library)
    return NS_ERROR_FAILURE;

  NPPluginFuncs* pluginFunctions = mPlugin->PluginFuncs();

  PRInt16 tmpResult = kNPEventNotHandled;

  if (pluginFunctions->event) {
    mCurrentPluginEvent = event;
#if defined(XP_WIN) || defined(XP_OS2)
    NS_TRY_SAFE_CALL_RETURN(tmpResult, (*pluginFunctions->event)(&mNPP, event), library, this);
#else
    tmpResult = (*pluginFunctions->event)(&mNPP, event);
#endif
    NPP_PLUGIN_LOG(PLUGIN_LOG_NOISY,
      ("NPP HandleEvent called: this=%p, npp=%p, event=%p, return=%d\n", 
      this, &mNPP, event, tmpResult));

    if (result)
      *result = tmpResult;
    mCurrentPluginEvent = nsnull;
  }

  return NS_OK;
}

NS_IMETHODIMP nsNPAPIPluginInstance::GetValueFromPlugin(NPPVariable variable, void* value)
{
#if (MOZ_PLATFORM_MAEMO == 5)
  // The maemo flash plugin does not remember this.  It sets the
  // value, but doesn't support the get value.
  if (variable == NPPVpluginWindowlessLocalBool) {
    *(NPBool*)value = mWindowlessLocal;
    return NS_OK;
  }
#endif
  if (!mPlugin)
    return NS_ERROR_FAILURE;

  PluginLibrary* library = mPlugin->GetLibrary();
  if (!library)
    return NS_ERROR_FAILURE;

  NPPluginFuncs* pluginFunctions = mPlugin->PluginFuncs();

  nsresult rv = NS_ERROR_FAILURE;
  if (pluginFunctions->getvalue && RUNNING == mRunning) {
    PluginDestructionGuard guard(this);

    NS_TRY_SAFE_CALL_RETURN(rv, (*pluginFunctions->getvalue)(&mNPP, variable, value), library, this);
    NPP_PLUGIN_LOG(PLUGIN_LOG_NORMAL,
    ("NPP GetValue called: this=%p, npp=%p, var=%d, value=%d, return=%d\n", 
    this, &mNPP, variable, value, rv));
  }

  return rv;
}

nsNPAPIPlugin* nsNPAPIPluginInstance::GetPlugin()
{
  return mPlugin;
}

nsresult nsNPAPIPluginInstance::GetNPP(NPP* aNPP) 
{
  if (aNPP)
    *aNPP = &mNPP;
  else
    return NS_ERROR_NULL_POINTER;

  return NS_OK;
}

void
nsNPAPIPluginInstance::SetURI(nsIURI* uri)
{
  mURI = uri;
}

nsIURI*
nsNPAPIPluginInstance::GetURI()
{
  return mURI.get();
}

NPError nsNPAPIPluginInstance::SetWindowless(PRBool aWindowless)
{
  mWindowless = aWindowless;

  if (mMIMEType) {
      // bug 558434 - Prior to 3.6.4, we assumed windowless was transparent.
      // Silverlight apparently relied on this quirk, so we default to
      // transparent unless they specify otherwise after setting the windowless
      // property. (Last tested version: sl 3.0). 
      NS_NAMED_LITERAL_CSTRING(silverlight, "application/x-silverlight");
      if (!PL_strncasecmp(mMIMEType, silverlight.get(), silverlight.Length())) {
          mTransparent = PR_TRUE;
      }
  }

  return NPERR_NO_ERROR;
}

NPError nsNPAPIPluginInstance::SetWindowlessLocal(PRBool aWindowlessLocal)
{
  mWindowlessLocal = aWindowlessLocal;
  return NPERR_NO_ERROR;
}

NPError nsNPAPIPluginInstance::SetTransparent(PRBool aTransparent)
{
  mTransparent = aTransparent;
  return NPERR_NO_ERROR;
}

NPError nsNPAPIPluginInstance::SetWantsAllNetworkStreams(PRBool aWantsAllNetworkStreams)
{
  mWantsAllNetworkStreams = aWantsAllNetworkStreams;
  return NPERR_NO_ERROR;
}

#ifdef XP_MACOSX
void nsNPAPIPluginInstance::SetDrawingModel(NPDrawingModel aModel)
{
  mDrawingModel = aModel;
}

void nsNPAPIPluginInstance::SetEventModel(NPEventModel aModel)
{
  // the event model needs to be set for the object frame immediately
  nsCOMPtr<nsIPluginInstanceOwner> owner;
  GetOwner(getter_AddRefs(owner));
  if (!owner) {
    NS_WARNING("Trying to set event model without a plugin instance owner!");
    return;
  }

  owner->SetEventModel(aModel);
}

#endif

NS_IMETHODIMP nsNPAPIPluginInstance::GetDrawingModel(PRInt32* aModel)
{
#ifdef XP_MACOSX
  *aModel = (PRInt32)mDrawingModel;
  return NS_OK;
#else
  return NS_ERROR_FAILURE;
#endif
}

NS_IMETHODIMP
nsNPAPIPluginInstance::GetJSObject(JSContext *cx, JSObject** outObject)
{
  NPObject *npobj = nsnull;
  nsresult rv = GetValueFromPlugin(NPPVpluginScriptableNPObject, &npobj);
  if (NS_FAILED(rv) || !npobj)
    return NS_ERROR_FAILURE;

  *outObject = nsNPObjWrapper::GetNewOrUsed(&mNPP, cx, npobj);

  _releaseobject(npobj);

  return NS_OK;
}

NS_IMETHODIMP
nsNPAPIPluginInstance::DefineJavaProperties()
{
  NPObject *plugin_obj = nsnull;

  // The dummy Java plugin's scriptable object is what we want to
  // expose as window.Packages. And Window.Packages.java will be
  // exposed as window.java.

  // Get the scriptable plugin object.
  nsresult rv = GetValueFromPlugin(NPPVpluginScriptableNPObject, &plugin_obj);

  if (NS_FAILED(rv) || !plugin_obj) {
    return NS_ERROR_FAILURE;
  }

  // Get the NPObject wrapper for window.
  NPObject *window_obj = _getwindowobject(&mNPP);

  if (!window_obj) {
    _releaseobject(plugin_obj);

    return NS_ERROR_FAILURE;
  }

  NPIdentifier java_id = _getstringidentifier("java");
  NPIdentifier packages_id = _getstringidentifier("Packages");

  NPObject *java_obj = nsnull;
  NPVariant v;
  OBJECT_TO_NPVARIANT(plugin_obj, v);

  // Define the properties.

  bool ok = _setproperty(&mNPP, window_obj, packages_id, &v);
  if (ok) {
    ok = _getproperty(&mNPP, plugin_obj, java_id, &v);

    if (ok && NPVARIANT_IS_OBJECT(v)) {
      // Set java_obj so that we properly release it at the end of
      // this function.
      java_obj = NPVARIANT_TO_OBJECT(v);

      ok = _setproperty(&mNPP, window_obj, java_id, &v);
    }
  }

  _releaseobject(window_obj);
  _releaseobject(plugin_obj);
  _releaseobject(java_obj);

  if (!ok)
    return NS_ERROR_FAILURE;

  return NS_OK;
}

nsresult
nsNPAPIPluginInstance::SetCached(PRBool aCache)
{
  mCached = aCache;
  return NS_OK;
}

NS_IMETHODIMP
nsNPAPIPluginInstance::ShouldCache(PRBool* shouldCache)
{
  *shouldCache = mCached;
  return NS_OK;
}

NS_IMETHODIMP
nsNPAPIPluginInstance::IsWindowless(PRBool* isWindowless)
{
  *isWindowless = mWindowless;
  return NS_OK;
}

NS_IMETHODIMP
nsNPAPIPluginInstance::AsyncSetWindow(NPWindow* window)
{
  if (RUNNING != mRunning)
    return NS_OK;

  PluginDestructionGuard guard(this);

  if (!mPlugin)
    return NS_ERROR_FAILURE;

  PluginLibrary* library = mPlugin->GetLibrary();
  if (!library)
    return NS_ERROR_FAILURE;

  return library->AsyncSetWindow(&mNPP, window);
}

NS_IMETHODIMP
nsNPAPIPluginInstance::GetSurface(gfxASurface** aSurface)
{
  if (RUNNING != mRunning)
    return NS_OK;

  PluginDestructionGuard guard(this);

  if (!mPlugin)
    return NS_ERROR_FAILURE;

  PluginLibrary* library = mPlugin->GetLibrary();
  if (!library)
    return NS_ERROR_FAILURE;

  return library->GetSurface(&mNPP, aSurface);
}


NS_IMETHODIMP
nsNPAPIPluginInstance::NotifyPainted(void)
{
  if (RUNNING != mRunning)
    return NS_OK;

  PluginDestructionGuard guard(this);

  if (!mPlugin)
    return NS_ERROR_FAILURE;

  PluginLibrary* library = mPlugin->GetLibrary();
  if (!library)
    return NS_ERROR_FAILURE;

  return library->NotifyPainted(&mNPP);
}

NS_IMETHODIMP
nsNPAPIPluginInstance::UseAsyncPainting(PRBool* aIsAsync)
{
  if (!mUsePluginLayersPref) {
    *aIsAsync = mUsePluginLayersPref;
    return NS_OK;
  }

  PluginDestructionGuard guard(this);

  if (!mPlugin)
    return NS_ERROR_FAILURE;

  PluginLibrary* library = mPlugin->GetLibrary();
  if (!library)
    return NS_ERROR_FAILURE;

  return library->UseAsyncPainting(&mNPP, aIsAsync);
}

NS_IMETHODIMP
nsNPAPIPluginInstance::IsTransparent(PRBool* isTransparent)
{
  *isTransparent = mTransparent;
  return NS_OK;
}

NS_IMETHODIMP
nsNPAPIPluginInstance::GetFormValue(nsAString& aValue)
{
  aValue.Truncate();

  char *value = nsnull;
  nsresult rv = GetValueFromPlugin(NPPVformValue, &value);
  if (NS_FAILED(rv) || !value)
    return NS_ERROR_FAILURE;

  CopyUTF8toUTF16(value, aValue);

  // NPPVformValue allocates with NPN_MemAlloc(), which uses
  // nsMemory.
  nsMemory::Free(value);

  return NS_OK;
}

NS_IMETHODIMP
nsNPAPIPluginInstance::PushPopupsEnabledState(PRBool aEnabled)
{
  nsCOMPtr<nsPIDOMWindow> window = GetDOMWindow();
  if (!window)
    return NS_ERROR_FAILURE;

  PopupControlState oldState =
    window->PushPopupControlState(aEnabled ? openAllowed : openAbused,
                                  PR_TRUE);

  if (!mPopupStates.AppendElement(oldState)) {
    // Appending to our state stack failed, pop what we just pushed.
    window->PopPopupControlState(oldState);
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsNPAPIPluginInstance::PopPopupsEnabledState()
{
  PRInt32 last = mPopupStates.Length() - 1;

  if (last < 0) {
    // Nothing to pop.
    return NS_OK;
  }

  nsCOMPtr<nsPIDOMWindow> window = GetDOMWindow();
  if (!window)
    return NS_ERROR_FAILURE;

  PopupControlState &oldState = mPopupStates[last];

  window->PopPopupControlState(oldState);

  mPopupStates.RemoveElementAt(last);
  
  return NS_OK;
}

NS_IMETHODIMP
nsNPAPIPluginInstance::GetPluginAPIVersion(PRUint16* version)
{
  NS_ENSURE_ARG_POINTER(version);

  if (!mPlugin)
    return NS_ERROR_FAILURE;

  if (!mPlugin->GetLibrary())
    return NS_ERROR_FAILURE;

  NPPluginFuncs* pluginFunctions = mPlugin->PluginFuncs();

  *version = pluginFunctions->version;

  return NS_OK;
}

nsresult
nsNPAPIPluginInstance::PrivateModeStateChanged()
{
  if (RUNNING != mRunning)
    return NS_OK;

  PLUGIN_LOG(PLUGIN_LOG_NORMAL, ("nsNPAPIPluginInstance informing plugin of private mode state change this=%p\n",this));

  if (!mPlugin)
    return NS_ERROR_FAILURE;

  PluginLibrary* library = mPlugin->GetLibrary();
  if (!library)
    return NS_ERROR_FAILURE;

  NPPluginFuncs* pluginFunctions = mPlugin->PluginFuncs();

  if (pluginFunctions->setvalue) {
    PluginDestructionGuard guard(this);
    
    nsCOMPtr<nsIPrivateBrowsingService> pbs = do_GetService(NS_PRIVATE_BROWSING_SERVICE_CONTRACTID);
    if (pbs) {
      PRBool pme = PR_FALSE;
      nsresult rv = pbs->GetPrivateBrowsingEnabled(&pme);
      if (NS_FAILED(rv))
        return rv;

      NPError error;
      NPBool value = static_cast<NPBool>(pme);
      NS_TRY_SAFE_CALL_RETURN(error, (*pluginFunctions->setvalue)(&mNPP, NPNVprivateModeBool, &value), library, this);
      return (error == NPERR_NO_ERROR) ? NS_OK : NS_ERROR_FAILURE;
    }
  }
  return NS_ERROR_FAILURE;
}

static void
PluginTimerCallback(nsITimer *aTimer, void *aClosure)
{
  nsNPAPITimer* t = (nsNPAPITimer*)aClosure;
  NPP npp = t->npp;
  uint32_t id = t->id;

  (*(t->callback))(npp, id);

  // Make sure we still have an instance and the timer is still alive
  // after the callback.
  nsNPAPIPluginInstance *inst = (nsNPAPIPluginInstance*)npp->ndata;
  if (!inst || !inst->TimerWithID(id, NULL))
    return;

  // use UnscheduleTimer to clean up if this is a one-shot timer
  PRUint32 timerType;
  t->timer->GetType(&timerType);
  if (timerType == nsITimer::TYPE_ONE_SHOT)
      inst->UnscheduleTimer(id);
}

nsNPAPITimer*
nsNPAPIPluginInstance::TimerWithID(uint32_t id, PRUint32* index)
{
  PRUint32 len = mTimers.Length();
  for (PRUint32 i = 0; i < len; i++) {
    if (mTimers[i]->id == id) {
      if (index)
        *index = i;
      return mTimers[i];
    }
  }
  return nsnull;
}

uint32_t
nsNPAPIPluginInstance::ScheduleTimer(uint32_t interval, NPBool repeat, void (*timerFunc)(NPP npp, uint32_t timerID))
{
  nsNPAPITimer *newTimer = new nsNPAPITimer();

  newTimer->npp = &mNPP;

  // generate ID that is unique to this instance
  uint32_t uniqueID = mTimers.Length();
  while ((uniqueID == 0) || TimerWithID(uniqueID, NULL))
    uniqueID++;
  newTimer->id = uniqueID;

  // create new xpcom timer, scheduled correctly
  nsresult rv;
  nsCOMPtr<nsITimer> xpcomTimer = do_CreateInstance(NS_TIMER_CONTRACTID, &rv);
  if (NS_FAILED(rv)) {
    delete newTimer;
    return 0;
  }
  const short timerType = (repeat ? (short)nsITimer::TYPE_REPEATING_SLACK : (short)nsITimer::TYPE_ONE_SHOT);
  xpcomTimer->InitWithFuncCallback(PluginTimerCallback, newTimer, interval, timerType);
  newTimer->timer = xpcomTimer;

  // save callback function
  newTimer->callback = timerFunc;

  // add timer to timers array
  mTimers.AppendElement(newTimer);

  return newTimer->id;
}

void
nsNPAPIPluginInstance::UnscheduleTimer(uint32_t timerID)
{
  // find the timer struct by ID
  PRUint32 index;
  nsNPAPITimer* t = TimerWithID(timerID, &index);
  if (!t)
    return;

  // cancel the timer
  t->timer->Cancel();

  // remove timer struct from array
  mTimers.RemoveElementAt(index);

  // delete timer
  delete t;
}

// Show the context menu at the location for the current event.
// This can only be called from within an NPP_SendEvent call.
NPError
nsNPAPIPluginInstance::PopUpContextMenu(NPMenu* menu)
{
  if (mOwner && mCurrentPluginEvent)
    return mOwner->ShowNativeContextMenu(menu, mCurrentPluginEvent);

  return NPERR_GENERIC_ERROR;
}

NPBool
nsNPAPIPluginInstance::ConvertPoint(double sourceX, double sourceY, NPCoordinateSpace sourceSpace,
                                    double *destX, double *destY, NPCoordinateSpace destSpace)
{
  if (mOwner)
    return mOwner->ConvertPoint(sourceX, sourceY, sourceSpace, destX, destY, destSpace);

  return PR_FALSE;
}

nsresult
nsNPAPIPluginInstance::GetDOMElement(nsIDOMElement* *result)
{
  if (!mOwner) {
    *result = nsnull;
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIPluginTagInfo> tinfo(do_QueryInterface(mOwner));
  if (tinfo)
    return tinfo->GetDOMElement(result);

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsNPAPIPluginInstance::InvalidateRect(NPRect *invalidRect)
{
  nsCOMPtr<nsIPluginInstanceOwner> owner;
  GetOwner(getter_AddRefs(owner));
  if (!owner)
    return NS_ERROR_FAILURE;

  return owner->InvalidateRect(invalidRect);
}

NS_IMETHODIMP
nsNPAPIPluginInstance::InvalidateRegion(NPRegion invalidRegion)
{
  nsCOMPtr<nsIPluginInstanceOwner> owner;
  GetOwner(getter_AddRefs(owner));
  if (!owner)
    return NS_ERROR_FAILURE;

  return owner->InvalidateRegion(invalidRegion);
}

NS_IMETHODIMP
nsNPAPIPluginInstance::ForceRedraw()
{
  nsCOMPtr<nsIPluginInstanceOwner> owner;
  GetOwner(getter_AddRefs(owner));
  if (!owner)
    return NS_ERROR_FAILURE;

  return owner->ForceRedraw();
}

NS_IMETHODIMP
nsNPAPIPluginInstance::GetMIMEType(const char* *result)
{
  if (!mMIMEType)
    *result = "";
  else
    *result = mMIMEType;

  return NS_OK;
}

NS_IMETHODIMP
nsNPAPIPluginInstance::GetJSContext(JSContext* *outContext)
{
  nsCOMPtr<nsIPluginInstanceOwner> owner;
  GetOwner(getter_AddRefs(owner));
  if (!owner)
    return NS_ERROR_FAILURE;

  *outContext = NULL;
  nsCOMPtr<nsIDocument> document;

  nsresult rv = owner->GetDocument(getter_AddRefs(document));

  if (NS_SUCCEEDED(rv) && document) {
    nsIScriptGlobalObject *global = document->GetScriptGlobalObject();

    if (global) {
      nsIScriptContext *context = global->GetContext();

      if (context) {
        *outContext = (JSContext*) context->GetNativeContext();
      }
    }
  }

  return rv;
}

NS_IMETHODIMP
nsNPAPIPluginInstance::GetOwner(nsIPluginInstanceOwner **aOwner)
{
  NS_ENSURE_ARG_POINTER(aOwner);
  *aOwner = mOwner;
  NS_IF_ADDREF(mOwner);
  return (mOwner ? NS_OK : NS_ERROR_FAILURE);
}

NS_IMETHODIMP
nsNPAPIPluginInstance::SetOwner(nsIPluginInstanceOwner *aOwner)
{
  mOwner = aOwner;
  return NS_OK;
}

NS_IMETHODIMP
nsNPAPIPluginInstance::ShowStatus(const char* message)
{
  if (mOwner)
    return mOwner->ShowStatus(message);

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsNPAPIPluginInstance::InvalidateOwner()
{
  mOwner = nsnull;

  return NS_OK;
}

nsresult
nsNPAPIPluginInstance::AsyncSetWindow(NPWindow& window)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}
