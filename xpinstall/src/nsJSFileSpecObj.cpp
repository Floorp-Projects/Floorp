/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "jsapi.h"
#include "nscore.h"
#include "nsIScriptContext.h"

#include "nsString.h"
#include "nsInstall.h"
#include "nsJSFileSpecObj.h"

extern void ConvertJSValToStr(nsString&  aString,
                             JSContext* aContext,
                             jsval      aValue);

extern void ConvertStrToJSVal(const nsString& aProp,
                             JSContext* aContext,
                             jsval* aReturn);

extern PRBool ConvertJSValToBool(PRBool* aProp,
                                JSContext* aContext,
                                jsval aValue);

extern PRBool ConvertJSValToObj(nsISupports** aSupports,
                               REFNSIID aIID,
                               const nsString& aTypeName,
                               JSContext* aContext,
                               jsval aValue);



/***********************************************************************************/
// Native methods for FileSpecObj functions

/*
 * Native method fso_ToString
 */
PR_STATIC_CALLBACK(JSBool)
fso_ToString(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{

  nsInstallFolder *nativeThis = (nsInstallFolder*)JS_GetPrivate(cx, obj);
  nsAutoString stringReturned;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if(nsnull == nativeThis)
  {
    return JS_TRUE;
  }

  if(NS_FAILED( nativeThis->ToString(&stringReturned)))
    return JS_TRUE;


  JSString *jsstring =
    JS_NewUCStringCopyN(cx, NS_REINTERPRET_CAST(const jschar*,
                                                stringReturned.get()),
                        stringReturned.Length());

  // set the return value
  *rval = STRING_TO_JSVAL(jsstring);

  return JS_TRUE;
}


/*
 * Native method fso_AppendString
 */
PR_STATIC_CALLBACK(JSBool)
fso_AppendPath(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  return JS_TRUE;
}


/*
 * FileSpecObj destructor
 */
static void PR_CALLBACK FileSpecObjectCleanup(JSContext *cx, JSObject *obj)
{
    nsInstallFolder *nativeThis = (nsInstallFolder*)JS_GetPrivate(cx, obj);
    if (nativeThis != nsnull)
       delete nativeThis;
}

/***********************************************************************/
//
// class for FileObj
//
JSClass FileSpecObjectClass = {
  "FileSpecObject",
  JSCLASS_HAS_PRIVATE,
  JS_PropertyStub,
  JS_PropertyStub,
  JS_PropertyStub,
  JS_PropertyStub,
  JS_EnumerateStub,
  JS_ResolveStub,
  JS_ConvertStub,
  FileSpecObjectCleanup
};


//
// FileObj class methods
//
static JSFunctionSpec fileSpecObjMethods[] = 
{
  {"appendPath",       fso_AppendPath,     1},
  {"toString",         fso_ToString,       0},
  {0}
};


PRInt32  InitFileSpecObjectPrototype(JSContext *jscontext, 
                                      JSObject *global, 
                                      JSObject **fileSpecObjectPrototype)
{
  *fileSpecObjectPrototype  = JS_InitClass( jscontext,         // context
                                            global,            // global object
                                            nsnull,            // parent proto 
                                            &FileSpecObjectClass, // JSClass
                                            nsnull,            // JSNative ctor
                                            0,                 // ctor args
                                            nsnull,            // proto props
                                            fileSpecObjMethods,// proto funcs
                                            nsnull,            // ctor props (static)
                                            nsnull);           // ctor funcs (static)

  if (nsnull == *fileSpecObjectPrototype) 
  {
      return NS_ERROR_FAILURE;
  }

 
  return NS_OK;
}


