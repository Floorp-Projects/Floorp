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
#include "nsWinProfile.h"
#include "nsJSWinProfile.h"

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


static void PR_CALLBACK WinProfileCleanup(JSContext *cx, JSObject *obj)
{
    nsWinProfile *nativeThis = (nsWinProfile*)JS_GetPrivate(cx, obj);
    delete nativeThis;
}

/***********************************************************************************/
// Native mothods for WinProfile functions

//
// Native method GetString
//
PR_STATIC_CALLBACK(JSBool)
WinProfileGetString(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsWinProfile *nativeThis = (nsWinProfile*)JS_GetPrivate(cx, obj);
  nsString     nativeRet;
  nsAutoString b0;
  nsAutoString b1;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if(nsnull == nativeThis)
  {
    return JS_TRUE;
  }

  if(argc >= 2)                             
  {
    //  public int getString ( String section,
    //                         String key);

    ConvertJSValToStr(b0, cx, argv[0]);
    ConvertJSValToStr(b1, cx, argv[1]);

    nativeThis->GetString(b0, b1, &nativeRet);

    ConvertStrToJSVal(nativeRet, cx, rval);
  }
  else
  {
    JS_ReportError(cx, "WinProfile.getString() parameters error");
    return JS_FALSE;
  }

  return JS_TRUE;
}

//
// Native method WriteString
//
PR_STATIC_CALLBACK(JSBool)
WinProfileWriteString(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsWinProfile *nativeThis = (nsWinProfile*)JS_GetPrivate(cx, obj);
  PRInt32 nativeRet;
  nsAutoString b0;
  nsAutoString b1;
  nsAutoString b2;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if(nsnull == nativeThis)
  {
    return JS_TRUE;
  }

  if(argc >= 3)
  {
    //  public int writeString ( String section,
    //                           String key,
    //                           String value);

    ConvertJSValToStr(b0, cx, argv[0]);
    ConvertJSValToStr(b1, cx, argv[1]);
    ConvertJSValToStr(b2, cx, argv[2]);

    if(NS_OK != nativeThis->WriteString(b0, b1, b2, &nativeRet))
    {
      return JS_FALSE;
    }

    *rval = INT_TO_JSVAL(nativeRet);
  }
  else
  {
    JS_ReportError(cx, "WinProfile.writeString() parameters error");
    return JS_FALSE;
  }

  return JS_TRUE;
}

//
// WinProfile constructor
//
PR_STATIC_CALLBACK(JSBool)
WinProfile(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  return JS_FALSE;
}

/***********************************************************************/
//
// class for WinProfile
//
JSClass WinProfileClass = {
  "WinProfile",
  JSCLASS_HAS_PRIVATE,
  JS_PropertyStub,
  JS_PropertyStub,
  JS_PropertyStub,
  JS_PropertyStub,
  JS_EnumerateStub,
  JS_ResolveStub,
  JS_ConvertStub,
  WinProfileCleanup
};


static JSConstDoubleSpec winprofile_constants[] = 
{
    {0}
};

//
// WinProfile class methods
//
static JSFunctionSpec WinProfileMethods[] = 
{
  {"getString",                  WinProfileGetString,                 2},
  {"writeString",                WinProfileWriteString,               3},
  {0}
};

PRInt32
InitWinProfilePrototype(JSContext *jscontext, JSObject *global, JSObject **winProfilePrototype)
{
  *winProfilePrototype = JS_InitClass( jscontext,          // context
                                       global,             // global object
                                       nsnull,             // parent proto 
                                       &WinProfileClass,   // JSClass
                                       nsnull,             // JSNative ctor
                                       0,                  // ctor args
                                       nsnull,             // proto props
                                       nsnull,             // proto funcs
                                       nsnull,             // ctor props (static)
                                       WinProfileMethods); // ctor funcs (static)

  if(nsnull == *winProfilePrototype) 
  {
    return NS_ERROR_FAILURE;
  }

  if(PR_FALSE == JS_DefineConstDoubles(jscontext, *winProfilePrototype, winprofile_constants))
    return NS_ERROR_FAILURE;

  return NS_OK;
}
