/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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

#include "nsJSNPRuntime.h"
#include "ns4xPlugin.h"
#include "ns4xPluginInstance.h"
#include "nsIPluginInstancePeer2.h"
#include "nsPIPluginInstancePeer.h"
#include "nsIScriptGlobalObject.h"
#include "nsIScriptContext.h"
#include "nsIDocument.h"
#include "nsIJSRuntimeService.h"
#include "nsIJSContextStack.h"
#include "prmem.h"

// Hash of NPObject wrappers that wrap JSObjects.
static PLDHashTable sJSObjWrappers;

// Hash of JSObject wrappers that wrap NPObjects.
static PLDHashTable sNPObjWrappers;

// Global wrapper count. This includes JSObject wrappers *and*
// NPObject wrappers. When this count goes to zero, there are no more
// wrappers and we can kill off hash tables etc.
static PRInt32 sWrapperCount;

// The JSRuntime. Used to unroot JSObjects when no JSContext is
// reachable.
static JSRuntime *sJSRuntime;

// The JS context stack, we use this to push a plugin's JSContext onto
// while executing JS on the context.
static nsIJSContextStack *sContextStack;

class AutoCXPusher
{
public:
  AutoCXPusher(JSContext *cx)
  {
    if (sContextStack)
      sContextStack->Push(cx);
  }

  ~AutoCXPusher()
  {
    if (sContextStack)
      sContextStack->Pop(nsnull);
  }
};

NPClass nsJSObjWrapper::sJSObjWrapperNPClass =
  {
    NP_CLASS_STRUCT_VERSION,
    nsJSObjWrapper::NP_Allocate,
    nsJSObjWrapper::NP_Deallocate,
    nsJSObjWrapper::NP_Invalidate,
    nsJSObjWrapper::NP_HasMethod,
    nsJSObjWrapper::NP_Invoke,
    nsJSObjWrapper::NP_HasProperty,
    nsJSObjWrapper::NP_GetProperty,
    nsJSObjWrapper::NP_SetProperty,
    nsJSObjWrapper::NP_RemoveProperty
  };

JS_STATIC_DLL_CALLBACK(JSBool)
NPObjWrapper_AddProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp);

JS_STATIC_DLL_CALLBACK(JSBool)
NPObjWrapper_DelProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp);

JS_STATIC_DLL_CALLBACK(JSBool)
NPObjWrapper_SetProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp);

JS_STATIC_DLL_CALLBACK(JSBool)
NPObjWrapper_GetProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp);

JS_STATIC_DLL_CALLBACK(JSBool)
NPObjWrapper_NewResolve(JSContext *cx, JSObject *obj, jsval id, uintN flags,
                        JSObject **objp);

JS_STATIC_DLL_CALLBACK(void)
NPObjWrapper_Finalize(JSContext *cx, JSObject *obj);

static JSClass sNPObjectJSWrapperClass =
  {
    "NPObject JS wrapper class", JSCLASS_HAS_PRIVATE | JSCLASS_NEW_RESOLVE,
    NPObjWrapper_AddProperty, NPObjWrapper_DelProperty,
    NPObjWrapper_GetProperty, NPObjWrapper_SetProperty, JS_EnumerateStub,
    (JSResolveOp)NPObjWrapper_NewResolve, JS_ConvertStub,
    NPObjWrapper_Finalize, nsnull, nsnull, nsnull, nsnull, nsnull, nsnull
  };


static void
OnWrapperCreated()
{
  if (sWrapperCount++ == 0) {
    static const char rtsvc_id[] = "@mozilla.org/js/xpc/RuntimeService;1";
    nsCOMPtr<nsIJSRuntimeService> rtsvc(do_GetService(rtsvc_id));
    if (!rtsvc)
      return;

    rtsvc->GetRuntime(&sJSRuntime);
    NS_ASSERTION(sJSRuntime != nsnull, "no JSRuntime?!");

    CallGetService("@mozilla.org/js/xpc/ContextStack;1", &sContextStack);
  }
}

static void
OnWrapperDestroyed()
{
  NS_ASSERTION(sWrapperCount, "Whaaa, unbalanced created/destroyed calls!");

  if (--sWrapperCount == 0) {
    if (sJSObjWrappers.ops) {
      NS_ASSERTION(sJSObjWrappers.entryCount == 0, "Uh, hash not empty?");

      // No more wrappers, and our hash was initalized. Finish the
      // hash to prevent leaking it.
      PL_DHashTableFinish(&sJSObjWrappers);

      sJSObjWrappers.ops = nsnull;
    }

    if (sNPObjWrappers.ops) {
      NS_ASSERTION(sNPObjWrappers.entryCount == 0, "Uh, hash not empty?");

      // No more wrappers, and our hash was initalized. Finish the
      // hash to prevent leaking it.
      PL_DHashTableFinish(&sNPObjWrappers);

      sNPObjWrappers.ops = nsnull;
    }

    // No more need for this.
    sJSRuntime = nsnull;

    NS_IF_RELEASE(sContextStack);
  }
}

static JSContext *
GetJSContext(NPP npp)
{
  ns4xPluginInstance *inst = (ns4xPluginInstance *)npp->ndata;
  NS_ENSURE_TRUE(inst, nsnull);

  nsCOMPtr<nsIPluginInstancePeer> pip;
  inst->GetPeer(getter_AddRefs(pip));
  nsCOMPtr<nsPIPluginInstancePeer> pp (do_QueryInterface(pip));
  NS_ENSURE_TRUE(pp, nsnull);

  nsCOMPtr<nsIPluginInstanceOwner> owner;
  pp->GetOwner(getter_AddRefs(owner));
  NS_ENSURE_TRUE(owner, nsnull);

  nsCOMPtr<nsIDocument> doc;
  owner->GetDocument(getter_AddRefs(doc));
  NS_ENSURE_TRUE(doc, nsnull);

  nsIScriptGlobalObject *sgo = doc->GetScriptGlobalObject();
  NS_ENSURE_TRUE(sgo, nsnull);

  nsIScriptContext *scx = sgo->GetContext();
  NS_ENSURE_TRUE(scx, nsnull);

  return (JSContext *)scx->GetNativeContext();
}


static NPP
FindNPPForJSObject(JSObject *obj, NPObject *npobj);


static jsval
NPVariantToJSVal(NPP npp, JSContext *cx, const NPVariant *variant)
{
  switch (variant->type) {
  case NPVariantType_Void :
    return JSVAL_VOID;
  case NPVariantType_Null :
    return JSVAL_NULL;
  case NPVariantType_Bool :
    return BOOLEAN_TO_JSVAL(NPVARIANT_TO_BOOLEAN(*variant));
  case NPVariantType_Int32 :
    return INT_TO_JSVAL(NPVARIANT_TO_INT32(*variant));
  case NPVariantType_Double :
    {
      jsval val;
      if (::JS_NewNumberValue(cx, NPVARIANT_TO_DOUBLE(*variant), &val)) {
        return val;
      }

      break;
    }
  case NPVariantType_String :
    {
      const NPString *s = &NPVARIANT_TO_STRING(*variant);
      PRUint32 len;
      PRUnichar *p =
        UTF8ToNewUnicode(nsDependentCString(s->utf8characters, s->utf8length),
                         &len);

      JSString *str = ::JS_NewUCString(cx, (jschar *)p, len);

      if (str) {
        return STRING_TO_JSVAL(str);
      }

      break;
    }
  case NPVariantType_Object:
    {
      if (npp) {
        JSObject *obj =
          nsNPObjWrapper::GetNewOrUsed(npp, cx, NPVARIANT_TO_OBJECT(*variant));

        if (obj) {
          return OBJECT_TO_JSVAL(obj);
        }
      }

      NS_ERROR("Error wrapping NPObject!");

      break;
    }
  default:
    NS_ERROR("Unknown NPVariant type!");
  }

  NS_ERROR("Unable to convert NPVariant to jsval!");

  return JSVAL_VOID;
}

bool
JSValToNPVariant(NPP npp, JSContext *cx, jsval val, NPVariant *variant)
{
  if (JSVAL_IS_PRIMITIVE(val)) {
    if (val == JSVAL_VOID) {
      VOID_TO_NPVARIANT(*variant);
    } else if (JSVAL_IS_NULL(val)) {
      NULL_TO_NPVARIANT(*variant);
    } else if (JSVAL_IS_BOOLEAN(val)) {
      BOOLEAN_TO_NPVARIANT(JSVAL_TO_BOOLEAN(val), *variant);
    } else if (JSVAL_IS_INT(val)) {
      INT32_TO_NPVARIANT(JSVAL_TO_INT(val), *variant);
    } else if (JSVAL_IS_DOUBLE(val)) {
      DOUBLE_TO_NPVARIANT(*JSVAL_TO_DOUBLE(val), *variant);
    } else if (JSVAL_IS_STRING(val)) {
      JSString *jsstr = JSVAL_TO_STRING(val);
      nsDependentString str((PRUnichar *)::JS_GetStringChars(jsstr),
                            ::JS_GetStringLength(jsstr));

      PRUint32 len;
      char *p = ToNewUTF8String(str, &len);

      if (!p) {
        return false;
      }

      STRINGN_TO_NPVARIANT(p, len, *variant);
    } else {
      NS_ERROR("Unknown primitive type!");

      return false;
    }

    return true;
  }

  if (!npp) {
    npp = FindNPPForJSObject(JSVAL_TO_OBJECT(val), nsnull);

    if (!npp) {
      NS_ERROR("Must have an NPP to wrap a JSObject!");

      return false;
    }
  }

  NPObject *npobj = nsJSObjWrapper::GetNewOrUsed(npp, cx,
                                                 JSVAL_TO_OBJECT(val));
  if (!npobj) {
    return false;
  }

  // Pass over ownership of npobj to *variant
  OBJECT_TO_NPVARIANT(npobj, *variant);

  return true;
}

static void
ThrowJSException(JSContext *cx, const char *message)
{
  // Add exception throwing code here.

  printf ("JS Error: %s\n", message);
}


nsJSObjWrapper::nsJSObjWrapper()
  : mJSObj(nsnull), mCx(nsnull), mNpp(nsnull)
{
  OnWrapperCreated();
}

nsJSObjWrapper::~nsJSObjWrapper()
{
  // Invalidate first, since it relies on sJSRuntime and sJSObjWrappers.
  NP_Invalidate(this);

  OnWrapperDestroyed();
}

// static
NPObject *
nsJSObjWrapper::NP_Allocate()
{
  return new nsJSObjWrapper;
}

// static
void
nsJSObjWrapper::NP_Deallocate(NPObject *npobj)
{
  // nsJSObjWrapper::~nsJSObjWrapper() will call NP_Invalidate().
  delete (nsJSObjWrapper *)npobj;
}

// static
void
nsJSObjWrapper::NP_Invalidate(NPObject *npobj)
{
  nsJSObjWrapper *jsnpobj = (nsJSObjWrapper *)npobj;

  if (jsnpobj && jsnpobj->mJSObj) {
    // Unroot the object's JSObject
    ::JS_RemoveRootRT(sJSRuntime, &jsnpobj->mJSObj);

    if (sJSObjWrappers.ops) {
      // Remove the wrapper from the hash
      PL_DHashTableOperate(&sJSObjWrappers, jsnpobj->mJSObj, PL_DHASH_REMOVE);
    }

    // Forget our reference to the JSObject etc.
    jsnpobj->mJSObj = nsnull;
    jsnpobj->mCx = nsnull;
    jsnpobj->mNpp = nsnull;
  }
}

static JSBool
GetProperty(JSContext *cx, JSObject *obj, NPIdentifier identifier, jsval *rval)
{
  jsval id = (jsval)identifier;

  AutoCXPusher pusher(cx);

  if (JSVAL_IS_STRING(id)) {
    JSString *str = JSVAL_TO_STRING(id);

    return ::JS_GetUCProperty(cx, obj, ::JS_GetStringChars(str),
                              ::JS_GetStringLength(str), rval);
  }

  NS_ASSERTION(JSVAL_IS_INT(id), "id must be either string or int!\n");

  return ::JS_GetElement(cx, obj, JSVAL_TO_INT(id), rval);
}

// static
bool
nsJSObjWrapper::NP_HasMethod(NPObject *npobj, NPIdentifier identifier)
{
  if (!npobj) {
    return PR_FALSE;
  }

  nsJSObjWrapper *npjsobj = (nsJSObjWrapper *)npobj;
  jsval v;
  JSBool ok = GetProperty(npjsobj->mCx, npjsobj->mJSObj, identifier, &v);

  return ok && !JSVAL_IS_PRIMITIVE(v) &&
    ::JS_ObjectIsFunction(npjsobj->mCx, JSVAL_TO_OBJECT(v));
}

// static
bool
nsJSObjWrapper::NP_Invoke(NPObject *npobj, NPIdentifier identifier,
                          const NPVariant *args, uint32_t argCount,
                          NPVariant *result)
{
  if (!npobj || !result) {
    // XXX: Throw null-ptr exception

    return PR_FALSE;
  }

  // Initialize *result
  VOID_TO_NPVARIANT(*result);

  nsJSObjWrapper *npjsobj = (nsJSObjWrapper *)npobj;
  jsval fv;

  AutoCXPusher pusher(npjsobj->mCx);

  if (!GetProperty(npjsobj->mCx, npjsobj->mJSObj, identifier, &fv) ||
      ::JS_TypeOfValue(npjsobj->mCx, fv) != JSTYPE_FUNCTION) {
    return PR_FALSE;
  }

  jsval jsargs_buf[8];
  jsval *jsargs = jsargs_buf;

  if (argCount > (sizeof(jsargs_buf) / sizeof(jsval))) {
    // Our stack buffer isn't large enough to hold all arguments,
    // malloc a buffer.
    jsargs = (jsval *)PR_Malloc(argCount * sizeof(jsval));

    if (!jsargs) {
      // XXX: throw an OOM exception!

      return PR_FALSE;
    }
  }

  // Convert args
  for (PRUint32 i = 0; i < argCount; ++i) {
    jsargs[i] = NPVariantToJSVal(npjsobj->mNpp, npjsobj->mCx, args + i);
  }

  jsval v;
  JSBool ok = ::JS_CallFunctionValue(npjsobj->mCx, npjsobj->mJSObj, fv,
                                     argCount, jsargs, &v);

  if (jsargs != jsargs_buf)
    PR_Free(jsargs);

  if (ok)
    ok = JSValToNPVariant(npjsobj->mNpp, npjsobj->mCx, v, result);

  // return ok == JS_TRUE to quiet down compiler warning, even if
  // return ok is what we really want.
  return ok == JS_TRUE;
}

// static
bool
nsJSObjWrapper::NP_HasProperty(NPObject *npobj, NPIdentifier identifier)
{
  if (!npobj) {
    return PR_FALSE;
  }

  nsJSObjWrapper *npjsobj = (nsJSObjWrapper *)npobj;
  jsval id = (jsval)identifier;
  jsval found;
  JSBool ok = JS_FALSE;

  if (JSVAL_IS_STRING(id)) {
    JSString *str = JSVAL_TO_STRING(id);

    ok = ::JS_LookupUCProperty(npjsobj->mCx, npjsobj->mJSObj,
                               ::JS_GetStringChars(str),
                               ::JS_GetStringLength(str), &found);
  } else {
    NS_ASSERTION(JSVAL_IS_INT(id), "id must be either string or int!\n");

    ok = ::JS_LookupElement(npjsobj->mCx, npjsobj->mJSObj, JSVAL_TO_INT(id),
                            &found);
  }

  return ok && !JSVAL_IS_VOID(found);
}

// static
bool
nsJSObjWrapper::NP_GetProperty(NPObject *npobj, NPIdentifier identifier,
                               NPVariant *result)
{
  if (!npobj)
    return PR_FALSE;

  nsJSObjWrapper *npjsobj = (nsJSObjWrapper *)npobj;

  AutoCXPusher pusher(npjsobj->mCx);

  jsval v;
  return GetProperty(npjsobj->mCx, npjsobj->mJSObj, identifier, &v) &&
    JSValToNPVariant(npjsobj->mNpp, npjsobj->mCx, v, result);
}

// static
bool
nsJSObjWrapper::NP_SetProperty(NPObject *npobj, NPIdentifier identifier,
                               const NPVariant *value)
{
  if (!npobj)
    return PR_FALSE;

  nsJSObjWrapper *npjsobj = (nsJSObjWrapper *)npobj;
  jsval id = (jsval)identifier;
  JSBool ok = JS_FALSE;

  AutoCXPusher pusher(npjsobj->mCx);

  jsval v = NPVariantToJSVal(npjsobj->mNpp, npjsobj->mCx, value);

  if (JSVAL_IS_STRING(id)) {
    JSString *str = JSVAL_TO_STRING(id);

    ok = ::JS_SetUCProperty(npjsobj->mCx, npjsobj->mJSObj,
                            ::JS_GetStringChars(str),
                            ::JS_GetStringLength(str), &v);
  } else {
    NS_ASSERTION(JSVAL_IS_INT(id), "id must be either string or int!\n");

    ok = ::JS_SetElement(npjsobj->mCx, npjsobj->mJSObj, JSVAL_TO_INT(id), &v);
  }

  // return ok == JS_TRUE to quiet down compiler warning, even if
  // return ok is what we really want.
  return ok == JS_TRUE;
}

// static
bool
nsJSObjWrapper::NP_RemoveProperty(NPObject *npobj, NPIdentifier identifier)
{
  if (!npobj)
    return PR_FALSE;

  nsJSObjWrapper *npjsobj = (nsJSObjWrapper *)npobj;
  jsval id = (jsval)identifier;
  JSBool ok = JS_FALSE;

  AutoCXPusher pusher(npjsobj->mCx);

  if (JSVAL_IS_STRING(id)) {
    JSString *str = JSVAL_TO_STRING(id);

    jsval unused;
    ok = ::JS_DeleteUCProperty2(npjsobj->mCx, npjsobj->mJSObj,
                                ::JS_GetStringChars(str),
                                ::JS_GetStringLength(str), &unused);
  } else {
    NS_ASSERTION(JSVAL_IS_INT(id), "id must be either string or int!\n");

    ok = ::JS_DeleteElement(npjsobj->mCx, npjsobj->mJSObj, JSVAL_TO_INT(id));
  }

  // return ok == JS_TRUE to quiet down compiler warning, even if
  // return ok is what we really want.
  return ok == JS_TRUE;
}

class JSObjWrapperHashEntry : public PLDHashEntryHdr
{
public:
  nsJSObjWrapper *mNPObj;
};


PR_STATIC_CALLBACK(const void *)
NPObjWrapperHashGetKey(PLDHashTable *table, PLDHashEntryHdr *entry)
{
  JSObjWrapperHashEntry *e =
    NS_STATIC_CAST(JSObjWrapperHashEntry *, entry);

  return e->mNPObj->mJSObj;
}

PR_STATIC_CALLBACK(PRBool)
NPObjWrapperHashMatchEntry(PLDHashTable *table, const PLDHashEntryHdr *entry,
                           const void *key)
{
  const JSObjWrapperHashEntry *e =
    NS_STATIC_CAST(const JSObjWrapperHashEntry *, entry);

  return e->mNPObj->mJSObj == key;
}


// Look up or create an NPObject that wraps the JSObject obj.

// static
NPObject *
nsJSObjWrapper::GetNewOrUsed(NPP npp, JSContext *cx, JSObject *obj)
{
  if (!npp) {
    NS_ERROR("Null NPP passed to nsJSObjWrapper::GetNewOrUsed()!");

    return nsnull;
  }

  if (!cx) {
    cx = GetJSContext(npp);

    if (!cx) {
      NS_ERROR("Unable to find a JSContext in "
               "nsJSObjWrapper::GetNewOrUsed()!");

      return nsnull;
    }
  }

  JSClass *clazz = JS_GET_CLASS(cx, obj);

  if (clazz == &sNPObjectJSWrapperClass) {
    // obj is one of our own, its private data is the NPObject we're
    // looking for.

    NPObject *npobj = (NPObject *)::JS_GetPrivate(cx, obj);

    return _retainobject(npobj);
  }

  if (!sJSObjWrappers.ops) {
    // No hash yet (or any more), initalize it.

    static PLDHashTableOps ops =
      {
        PL_DHashAllocTable,
        PL_DHashFreeTable,
        NPObjWrapperHashGetKey,
        PL_DHashVoidPtrKeyStub,
        NPObjWrapperHashMatchEntry,
        PL_DHashMoveEntryStub,
        PL_DHashClearEntryStub,
        PL_DHashFinalizeStub
      };

    if (!PL_DHashTableInit(&sJSObjWrappers, &ops, nsnull,
                           sizeof(JSObjWrapperHashEntry), 16)) {
      NS_ERROR("Error initializing PLDHashTable!");

      return nsnull;
    }
  }

  JSObjWrapperHashEntry *entry =
    NS_STATIC_CAST(JSObjWrapperHashEntry *,
                   PL_DHashTableOperate(&sJSObjWrappers, obj, PL_DHASH_ADD));

  if (PL_DHASH_ENTRY_IS_BUSY(entry) && entry->mNPObj) {
    // Found a live nsJSObjWrapper, return it.

    return _retainobject(entry->mNPObj);
  }

  // No existing nsJSObjWrapper, create one.

  nsJSObjWrapper *npobj =
    (nsJSObjWrapper *)_createobject(&sJSObjWrapperNPClass);

  if (!npobj) {
    // OOM? Remove the stale entry from the hash.

    PL_DHashTableRawRemove(&sJSObjWrappers, entry);

    return nsnull;
  }

  entry->mNPObj = npobj;

  npobj->mJSObj = obj;
  npobj->mCx = cx;
  npobj->mNpp = npp;

  // Root the JSObject, its lifetime is now tied to that of the
  // NPObject.
  if (!::JS_AddNamedRoot(cx, &npobj->mJSObj, "nsJSObjWrapper::mJSObject")) {
    NS_ERROR("Failed to root JSObject!");

    _releaseobject(npobj);

    PL_DHashTableRawRemove(&sJSObjWrappers, entry);

    return nsnull;
  }

  return npobj;
}

static NPObject *
GetNPObject(JSContext *cx, JSObject *obj)
{
  while (JS_GET_CLASS(cx, obj) != &sNPObjectJSWrapperClass) {
    obj = ::JS_GetPrototype(cx, obj);
  }

  if (!obj) {
    return nsnull;
  }

  return (NPObject *)::JS_GetPrivate(cx, obj);
}

JS_STATIC_DLL_CALLBACK(JSBool)
NPObjWrapper_AddProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  NPObject *npobj = GetNPObject(cx, obj);

  if (!npobj || !npobj->_class || !npobj->_class->hasProperty ||
      !npobj->_class->hasMethod) {
    ThrowJSException(cx, "Bad NPObject as private data!");

    return JS_FALSE;
  }

  // We must permit methods here since JS_DefineUCFunction() will add
  // the function as a property
  if (!npobj->_class->hasProperty(npobj, (NPIdentifier)id) &&
      !npobj->_class->hasMethod(npobj, (NPIdentifier)id)) {
    ThrowJSException(cx, "Trying to add unsupported property on scriptable "
                     "plugin object!");

    return JS_FALSE;
  }

  return JS_TRUE;
}

JS_STATIC_DLL_CALLBACK(JSBool)
NPObjWrapper_DelProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  NPObject *npobj = GetNPObject(cx, obj);

  if (!npobj || !npobj->_class || !npobj->_class->hasProperty) {
    ThrowJSException(cx, "Bad NPObject as private data!");

    return JS_FALSE;
  }

  if (!npobj->_class->hasProperty(npobj, (NPIdentifier)id)) {
    ThrowJSException(cx, "Trying to remove unsupported property on scriptable "
                     "plugin object!");

    return JS_FALSE;
  }

  return JS_TRUE;
}

JS_STATIC_DLL_CALLBACK(JSBool)
NPObjWrapper_SetProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  NPObject *npobj = GetNPObject(cx, obj);

  if (!npobj || !npobj->_class || !npobj->_class->hasProperty ||
      !npobj->_class->setProperty) {
    ThrowJSException(cx, "Bad NPObject as private data!");

    return JS_FALSE;
  }

  if (!npobj->_class->hasProperty(npobj, (NPIdentifier)id)) {
    ThrowJSException(cx, "Trying to set unsupported property on scriptable "
                     "plugin object!");

    return JS_FALSE;
  }

  NPVariant npv;
  if (!JSValToNPVariant(nsnull, cx, *vp, &npv)) {
    ThrowJSException(cx, "Error converting jsval to NPVariant!");

    return JS_FALSE;
  }

  JSBool ok = npobj->_class->setProperty(npobj, (NPIdentifier)id, &npv);

  // Release the variant
  _releasevariantvalue(&npv);

  if (!ok) {
    ThrowJSException(cx, "Error setting property on scriptable plugin "
                     "object!");
  }

  return ok;
}

JS_STATIC_DLL_CALLBACK(JSBool)
NPObjWrapper_GetProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  NPObject *npobj = GetNPObject(cx, obj);

  if (!npobj || !npobj->_class || !npobj->_class->hasProperty ||
      !npobj->_class->hasMethod || !npobj->_class->getProperty) {
    ThrowJSException(cx, "Bad NPObject as private data!");

    return JS_FALSE;
  }

  if (npobj->_class->hasProperty(npobj, (NPIdentifier)id)) {
    NPVariant npv;
    VOID_TO_NPVARIANT(npv);

    if (!npobj->_class->getProperty(npobj, (NPIdentifier)id, &npv)) {
      ThrowJSException(cx, "Error setting property on scriptable plugin "
                       "object!");

      return JS_FALSE;
    }

    NPP npp = nsnull;

    if (NPVARIANT_IS_OBJECT(npv)) {
      // We'll only need an npp if npv is an object, so don't waste time
      // looking it up in other cases.

      npp = FindNPPForJSObject(obj, npobj);
    }

    *vp = NPVariantToJSVal(npp, cx, &npv);

    // *vp now owns the value, release our reference.
    _releasevariantvalue(&npv);

    return JS_TRUE;
  }

  if (npobj->_class->hasMethod(npobj, (NPIdentifier)id)) {
    return PR_TRUE;
  }

  ThrowJSException(cx, "Trying to get unsupported property on scriptable "
                   "plugin object!");

  return JS_FALSE;
}

JS_STATIC_DLL_CALLBACK(JSBool)
CallNPMethod(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
             jsval *rval)
{
  while (JS_GET_CLASS(cx, obj) != &sNPObjectJSWrapperClass) {
    obj = ::JS_GetPrototype(cx, obj);
  }

  if (!obj) {
    ThrowJSException(cx, "NPMethod called on non-NPObject wrapped JSObject!");

    return JS_FALSE;
  }

  NPObject *npobj = (NPObject *)::JS_GetPrivate(cx, obj);

  if (!npobj || !npobj->_class || !npobj->_class->invoke) {
    ThrowJSException(cx, "Bad NPObject as private data!");

    return JS_FALSE;
  }

  NPP npp = FindNPPForJSObject(obj, npobj);

  if (!npp) {
    ThrowJSException(cx, "Error finding NPP for JSObject!");

    return JS_FALSE;
  }

  JSObject *funobj = JSVAL_TO_OBJECT(argv[-2]);
  JSFunction *fun = (JSFunction *)::JS_GetPrivate(cx, funobj);

  jsval method = STRING_TO_JSVAL(::JS_GetFunctionId(fun));

  NPVariant npargs_buf[8];
  NPVariant *npargs = npargs_buf;

  if (argc > (sizeof(npargs_buf) / sizeof(NPVariant))) {
    // Our stack buffer isn't large enough to hold all arguments,
    // malloc a buffer.
    npargs = (NPVariant *)PR_Malloc(argc * sizeof(NPVariant));

    if (!npargs) {
      ThrowJSException(cx, "Out of memory!");

      return JS_FALSE;
    }
  }

  // Convert arguments
  PRUint32 i;
  for (i = 0; i < argc; ++i) {
    if (!JSValToNPVariant(npp, cx, argv[i], npargs + i)) {
      ThrowJSException(cx, "Error converting jsvals to NPVariants!");

      return JS_FALSE;
    }
  }

  NPVariant v;
  VOID_TO_NPVARIANT(v);

  JSBool ok = npobj->_class->invoke(npobj, (NPIdentifier)method, npargs, argc,
                                    &v);

  // Release arguments.
  for (i = 0; i < argc; ++i) {
    _releasevariantvalue(npargs + i);
  }

  if (npargs != npargs_buf)
    PR_Free(npargs);

  if (!ok) {
    ThrowJSException(cx, "Error calling method on NPObject!");

    return JS_FALSE;
  }

  *rval = NPVariantToJSVal(npp, cx, &v);

  // *rval now owns the value, release our reference.
  _releasevariantvalue(&v);

  return JS_TRUE;
}


JS_STATIC_DLL_CALLBACK(JSBool)
NPObjWrapper_NewResolve(JSContext *cx, JSObject *obj, jsval id, uintN flags,
                        JSObject **objp)
{
  NPObject *npobj = GetNPObject(cx, obj);

  if (!npobj || !npobj->_class || !npobj->_class->hasProperty ||
      !npobj->_class->hasMethod) {
    ThrowJSException(cx, "Bad NPObject as private data!");

    return JS_FALSE;
  }

  if (npobj->_class->hasProperty(npobj, (NPIdentifier)id)) {
    *objp = obj;
  } else if (npobj->_class->hasMethod(npobj, (NPIdentifier)id)) {
    JSString *str = nsnull;

    if (JSVAL_IS_STRING(id)) {
      str = JSVAL_TO_STRING(id);
    } else {
      NS_ASSERTION(JSVAL_IS_INT(id), "id must be either string or int!\n");

      str = ::JS_ValueToString(cx, id);

      if (!str) {
        // OOM. The JS engine throws exceptions for us in this case.

        return JS_FALSE;
      }
    }

    JSFunction *fnc =
      ::JS_DefineUCFunction(cx, obj, ::JS_GetStringChars(str),
                            ::JS_GetStringLength(str), CallNPMethod, 0,
                            JSPROP_ENUMERATE);

    *objp = obj;

    return fnc != nsnull;
  }

  return JS_TRUE;
}

JS_STATIC_DLL_CALLBACK(void)
NPObjWrapper_Finalize(JSContext *cx, JSObject *obj)
{
  NPObject *npobj = (NPObject *)::JS_GetPrivate(cx, obj);

  if (npobj) {
    if (sNPObjWrappers.ops) {
      PL_DHashTableOperate(&sNPObjWrappers, npobj, PL_DHASH_REMOVE);
    }

    // Let go of our NPObject
    _releaseobject(npobj);
  }

  OnWrapperDestroyed();
}


class NPObjWrapperHashEntry : public PLDHashEntryHdr
{
public:
  NPObject *mNPObj; // Must be the first member for the PLDHash stubs to work
  JSObject *mJSObj;
  NPP mNpp;
};


// Look up or create a JSObject that wraps the NPObject npobj.

// static
JSObject *
nsNPObjWrapper::GetNewOrUsed(NPP npp, JSContext *cx, NPObject *npobj)
{
  if (!npobj) {
    NS_ERROR("Null NPObject passed to nsNPObjWrapper::GetNewOrUsed()!");

    return nsnull;
  }

  if (npobj->_class == &nsJSObjWrapper::sJSObjWrapperNPClass) {
    // npobj is one of our own, return its existing JSObject.

    return ((nsJSObjWrapper *)npobj)->mJSObj;
  }

  if (!npp) {
    NS_ERROR("No npp passed to nsNPObjWrapper::GetNewOrUsed()!");

    return nsnull;
  }

  if (!sNPObjWrappers.ops) {
    // No hash yet (or any more), initalize it.

    if (!PL_DHashTableInit(&sNPObjWrappers, PL_DHashGetStubOps(), nsnull,
                           sizeof(NPObjWrapperHashEntry), 16)) {
      NS_ERROR("Error initializing PLDHashTable!");

      return nsnull;
    }
  }

  NPObjWrapperHashEntry *entry =
    NS_STATIC_CAST(NPObjWrapperHashEntry *,
                   PL_DHashTableOperate(&sNPObjWrappers, npobj,
                                        PL_DHASH_ADD));

  if (PL_DHASH_ENTRY_IS_BUSY(entry) && entry->mJSObj) {
    // Found a live NPObject wrapper, return it.
    return entry->mJSObj;
  }

  entry->mNPObj = npobj;
  entry->mNpp = npp;

  // No existing JSObject, create one.

  JSObject *obj = ::JS_NewObject(cx, &sNPObjectJSWrapperClass, nsnull, nsnull);

  if (!obj) {
    // OOM? Remove the stale entry from the hash.

    PL_DHashTableRawRemove(&sJSObjWrappers, entry);

    return nsnull;
  }

  OnWrapperCreated();

  entry->mJSObj = obj;

  if (!::JS_SetPrivate(cx, obj, npobj)) {
    NS_ERROR("Error setting private NPObject data in JS wrapper!");

    PL_DHashTableRawRemove(&sJSObjWrappers, entry);

    return nsnull;
  }

  // The new JSObject now holds on to npobj
  _retainobject(npobj);

  return obj;
}


// PLDHashTable enumeration callbacks for destruction code.
PR_STATIC_CALLBACK(PLDHashOperator)
JSObjWrapperPluginDestroyedCallback(PLDHashTable *table, PLDHashEntryHdr *hdr,
                                    PRUint32 number, void *arg)
{
  JSObjWrapperHashEntry *entry = (JSObjWrapperHashEntry *)hdr;

  nsJSObjWrapper *npobj = entry->mNPObj;

  if (npobj->mNpp == arg) {
    // Prevent invalidate() and _releaseobject() from touching the hash
    // we're enumerating.
    const PLDHashTableOps *ops = table->ops;
    table->ops = nsnull;

    if (npobj->_class && npobj->_class->invalidate) {
      npobj->_class->invalidate(npobj);
    }

    _releaseobject(npobj);

    table->ops = ops;

    return PL_DHASH_REMOVE;
  }

  return PL_DHASH_NEXT;
}

PR_STATIC_CALLBACK(PLDHashOperator)
NPObjWrapperPluginDestroyedCallback(PLDHashTable *table, PLDHashEntryHdr *hdr,
                                    PRUint32 number, void *arg)
{
  NPObjWrapperHashEntry *entry = (NPObjWrapperHashEntry *)hdr;

  if (entry->mNpp == arg) {
    NPObject *npobj = entry->mNPObj;

    if (npobj->_class && npobj->_class->invalidate) {
      npobj->_class->invalidate(npobj);
    }

    // Force deallocation of plugin objects since the plugin they came
    // from is being torn down.
    if (npobj->_class && npobj->_class->deallocate) {
      npobj->_class->deallocate(npobj);
    } else {
      PR_Free(npobj);
    }

    JSContext *cx = GetJSContext((NPP)arg);

    ::JS_SetPrivate(cx, entry->mJSObj, nsnull);

    return PL_DHASH_REMOVE;
  }

  return PL_DHASH_NEXT;
}

// static
void
nsJSNPRuntime::OnPluginDestroy(NPP npp)
{
  if (sJSObjWrappers.ops) {
    PL_DHashTableEnumerate(&sJSObjWrappers,
                           JSObjWrapperPluginDestroyedCallback, npp);
  }

  if (sNPObjWrappers.ops) {
    PL_DHashTableEnumerate(&sNPObjWrappers,
                           NPObjWrapperPluginDestroyedCallback, npp);
  }
}

// Find the NPP for a JSObject or from its NPObject counter-part, if any.
static NPP
FindNPPForJSObject(JSObject *obj, NPObject *npobj)
{
  if (npobj) {
    if (npobj->_class == &nsJSObjWrapper::sJSObjWrapperNPClass) {
      nsJSObjWrapper *jsnpobj = (nsJSObjWrapper *)npobj;

      return jsnpobj->mNpp;
    }

    NPObjWrapperHashEntry *entry =
      NS_STATIC_CAST(NPObjWrapperHashEntry *,
                     PL_DHashTableOperate(&sNPObjWrappers, npobj,
                                          PL_DHASH_ADD));

    if (PL_DHASH_ENTRY_IS_BUSY(entry)) {
      return entry->mNpp;
    }
  }

  if (sNPObjWrappers.ops) {
    NPObjWrapperHashEntry *entry =
      NS_STATIC_CAST(NPObjWrapperHashEntry *,
                     PL_DHashTableOperate(&sNPObjWrappers, npobj,
                                          PL_DHASH_ADD));

    if (PL_DHASH_ENTRY_IS_BUSY(entry)) {
      return entry->mNpp;
    }
  }

  return nsnull;
}


