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
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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


static void PR_CALLBACK
WinProfileCleanup(JSContext *cx, JSObject *obj);

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
  nsWinProfile *nativeThis =
    (nsWinProfile*)JS_GetInstancePrivate(cx, obj, &WinProfileClass, argv);
  if (!nativeThis)
    return JS_FALSE;

  nsString     nativeRet;
  nsAutoString b0;
  nsAutoString b1;

  *rval = JSVAL_NULL;

  if(argc >= 2)                             
  {
    //  public string getString ( String section,
    //                         String key);

    ConvertJSValToStr(b0, cx, argv[0]);
    ConvertJSValToStr(b1, cx, argv[1]);

    nativeThis->GetString(b0, b1, &nativeRet);

    ConvertStrToJSVal(nativeRet, cx, rval);
  }
  else
  {
    JS_ReportWarning(cx, "WinProfile.getString() parameters error");
  }

  return JS_TRUE;
}


//
// Native method WriteString
//
PR_STATIC_CALLBACK(JSBool)
WinProfileWriteString(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsWinProfile *nativeThis =
    (nsWinProfile*)JS_GetInstancePrivate(cx, obj, &WinProfileClass, argv);
  if (!nativeThis)
    return JS_FALSE;

  PRInt32 nativeRet;
  nsAutoString b0;
  nsAutoString b1;
  nsAutoString b2;

  *rval = JSVAL_ZERO;

  if(argc >= 3)
  {
    //  public int writeString ( String section,
    //                           String key,
    //                           String value);

    ConvertJSValToStr(b0, cx, argv[0]);
    ConvertJSValToStr(b1, cx, argv[1]);
    ConvertJSValToStr(b2, cx, argv[2]);

    if(NS_OK == nativeThis->WriteString(b0, b1, b2, &nativeRet))
    {
      *rval = INT_TO_JSVAL(nativeRet);
    }
  }
  else
  {
    JS_ReportWarning(cx, "WinProfile.writeString() parameters error");
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

static JSConstDoubleSpec winprofile_constants[] = 
{
    {0}
};

//
// WinProfile class methods
//
static JSFunctionSpec WinProfileMethods[] = 
{
  {"getString",         WinProfileGetString,    2,0,0},
  {"writeString",       WinProfileWriteString,  3,0,0},
  {nsnull,nsnull,0,0,0}
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

