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
#include "nsJSUtils.h"
#include "nscore.h"
#include "nsIScriptContext.h"

#include "nsString.h"
#include "nsInstall.h"

#include "nsIDOMInstallVersion.h"

#ifdef XP_WIN
#include "nsWinReg.h"
#include "nsJSWinReg.h"

extern JSClass WinRegClass;
#endif

//
// Install property ids
//
enum Install_slots 
{
  INSTALL_USERPACKAGENAME = -1,
  INSTALL_REGPACKAGENAME  = -2,
  INSTALL_SILENT          = -3,
  INSTALL_JARFILE         = -4,
  INSTALL_FORCE           = -5,
  INSTALL_ARGUMENTS       = -6
};

/***********************************************************************/
//
// class for WinProfile
//
// JSClass WinProfileClass = {
//   "WinProfile",
//   JSCLASS_HAS_PRIVATE,
//   JS_PropertyStub,
//   JS_PropertyStub,
//   JS_PropertyStub,
//   JS_PropertyStub,
//   JS_EnumerateStub,
//   JS_ResolveStub,
//   JS_ConvertStub,
//   WinProfileCleanup
//};

/***********************************************************************/
//
// Install Properties Getter
//
PR_STATIC_CALLBACK(JSBool)
GetInstallProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsInstall *a = (nsInstall*)JS_GetPrivate(cx, obj);

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == a) {
    return JS_TRUE;
  }

  if (JSVAL_IS_INT(id)) 
  {
    switch(JSVAL_TO_INT(id)) {
      case INSTALL_USERPACKAGENAME:
      {
        nsAutoString prop;
        if (NS_OK == a->GetUserPackageName(prop)) 
        {
            *vp = STRING_TO_JSVAL( JS_NewUCStringCopyN(cx, prop.GetUnicode(), prop.Length()) );
        }
        else 
        {
          return JS_TRUE;
        }
        break;
      }
      case INSTALL_REGPACKAGENAME:
      {
        nsAutoString prop;
        if (NS_OK == a->GetRegPackageName(prop)) 
        {
          *vp = STRING_TO_JSVAL( JS_NewUCStringCopyN(cx, prop.GetUnicode(), prop.Length()) );
        }
        else 
        {
          return JS_TRUE;
        }
        break;
      }
      case INSTALL_JARFILE:
      {
        nsAutoString prop;
        
        a->GetJarFileLocation(prop);
        *vp = STRING_TO_JSVAL( JS_NewUCStringCopyN(cx, prop.GetUnicode(), prop.Length()) );
        
        break;
      }

      case INSTALL_ARGUMENTS:
      {
        nsAutoString prop;
        
        a->GetInstallArguments(prop); 
        *vp = STRING_TO_JSVAL( JS_NewUCStringCopyN(cx, prop.GetUnicode(), prop.Length()) );
        
        break;
      }
        
      default:
        return JS_TRUE;
    }
  }
  else {
    return JS_TRUE;
  }

  return PR_TRUE;
}

/***********************************************************************/
//
// Install Properties Setter
//
PR_STATIC_CALLBACK(JSBool)
SetInstallProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsInstall *a = (nsInstall*)JS_GetPrivate(cx, obj);

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == a) {
    return JS_TRUE;
  }

  if (JSVAL_IS_INT(id)) {
    switch(JSVAL_TO_INT(id)) {
      case 0:
      default:
          return JS_TRUE;
    }
  }
  else {
    return JS_TRUE;
  }

  return PR_TRUE;
}


static void PR_CALLBACK FinalizeInstall(JSContext *cx, JSObject *obj)
{
    nsInstall *nativeThis = (nsInstall*)JS_GetPrivate(cx, obj);
    delete nativeThis;
}

// static void PR_CALLBACK WinProfileCleanup(JSContext *cx, JSObject *obj)
// {
//     nsWinProfile *nativeThis = (nsWinProfile*)JS_GetPrivate(cx, obj);
//     delete nativeThis;
// }

void nsCvrtJSValToStr(nsString&  aString,
                      JSContext* aContext,
                      jsval      aValue)
{
  JSString *jsstring;
  if((jsstring = JS_ValueToString(aContext, aValue)) != nsnull)
  {
    aString.SetString(JS_GetStringChars(jsstring));
  }
  else
  {
    aString.Truncate();
  }
}

void nsCvrtStrToJSVal(const nsString& aProp,
                      JSContext* aContext,
                      jsval* aReturn)
{
  JSString *jsstring = JS_NewUCStringCopyN(aContext, aProp.GetUnicode(), aProp.Length());
  // set the return value
  *aReturn = STRING_TO_JSVAL(jsstring);
}

PRBool nsCvrtJSValToBool(PRBool* aProp,
                         JSContext* aContext,
                         jsval aValue)
{
  JSBool temp;
  if(JSVAL_IS_BOOLEAN(aValue) && JS_ValueToBoolean(aContext, aValue, &temp))
  {
    *aProp = (PRBool)temp;
  }
  else
  {
    JS_ReportError(aContext, "Parameter must be a boolean");
    return JS_FALSE;
  }

  return JS_TRUE;
}

PRBool nsCvrtJSValToObj(nsISupports** aSupports,
                        REFNSIID aIID,
                        const nsString& aTypeName,
                        JSContext* aContext,
                        jsval aValue)
{
  if (JSVAL_IS_NULL(aValue)) {
    *aSupports = nsnull;
  }
  else if (JSVAL_IS_OBJECT(aValue)) {
    JSObject* jsobj = JSVAL_TO_OBJECT(aValue); 
    JSClass* jsclass = JS_GetClass(aContext, jsobj);
    if ((nsnull != jsclass) && (jsclass->flags & JSCLASS_HAS_PRIVATE)) {
      nsISupports *supports = (nsISupports *)JS_GetPrivate(aContext, jsobj);
      if (NS_OK != supports->QueryInterface(aIID, (void **)aSupports)) {
        char buf[128];
        char typeName[128];
        aTypeName.ToCString(typeName, sizeof(typeName));
        sprintf(buf, "Parameter must of type %s", typeName);
        JS_ReportError(aContext, buf);
        return JS_FALSE;
      }
    }
    else {
      JS_ReportError(aContext, "Parameter isn't a object");
      return JS_FALSE;
    }
  }
  else {
    JS_ReportError(aContext, "Parameter must be an object");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method AbortInstall
//
PR_STATIC_CALLBACK(JSBool)
InstallAbortInstall(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsInstall *nativeThis = (nsInstall*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if(argc >= 0)
  {
    //  public int AbortInstall(void);

    if(NS_OK != nativeThis->AbortInstall())
    {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }
  else
  {
    JS_ReportError(cx, "Function AbortInstall requires 0 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method AddDirectory
//
PR_STATIC_CALLBACK(JSBool)
InstallAddDirectory(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsInstall *nativeThis = (nsInstall*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;
  PRInt32 nativeRet;
  nsAutoString b0;
  nsAutoString b1;
  nsAutoString b2;
  nsAutoString b3;
  nsAutoString b4;
  PRBool b5;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc == 1)                             
  {
    // public int AddDirectory (String jarSourcePath)

    nsCvrtJSValToStr(b0, cx, argv[0]);
       
    if(NS_OK != nativeThis->AddDirectory(b0, &nativeRet))
    {
      return JS_FALSE;
    }

    *rval = INT_TO_JSVAL(nativeRet);
  }
  else if (argc == 4)                             
  {
    //  public int AddDirectory ( String registryName,
    //                            String jarSourcePath,
    //                            String localDirSpec,
    //                            String relativeLocalPath); 

    nsCvrtJSValToStr(b0, cx, argv[0]);
    nsCvrtJSValToStr(b1, cx, argv[1]);
    nsCvrtJSValToStr(b2, cx, argv[2]);
    nsCvrtJSValToStr(b3, cx, argv[3]);

    if(NS_OK != nativeThis->AddDirectory(b0, b1, b2, b3, &nativeRet))
    {
      return JS_FALSE;
    }

    *rval = INT_TO_JSVAL(nativeRet);
  }
  else if (argc == 5)                             
  {
    //  public int AddDirectory ( String registryName,
    //                            String version,  --OR-- VersionInfo version
    //                            String jarSourcePath,
    //                            Object localDirSpec,
    //                            String relativeLocalPath); 

    nsCvrtJSValToStr(b0, cx, argv[0]);
    nsCvrtJSValToStr(b2, cx, argv[2]);
    nsCvrtJSValToStr(b3, cx, argv[3]);
    nsCvrtJSValToStr(b4, cx, argv[4]);

    if(JSVAL_IS_OBJECT(argv[1]))
    {
        JSObject* jsobj = JSVAL_TO_OBJECT(argv[1]);
        JSClass* jsclass = JS_GetClass(cx, jsobj);
        if ((nsnull != jsclass) && (jsclass->flags & JSCLASS_HAS_PRIVATE)) 
        {
          nsIDOMInstallVersion* version = (nsIDOMInstallVersion*)JS_GetPrivate(cx, jsobj);

          if(NS_OK != nativeThis->AddDirectory(b0, version, b2, b3, b4, &nativeRet))
          {
                return JS_FALSE;
          }
        }
    }
    else
    {
        nsCvrtJSValToStr(b1, cx, argv[1]);
        if(NS_OK != nativeThis->AddDirectory(b0, b1, b2, b3, b4, &nativeRet))
        {
            return JS_FALSE;
        }
    }

    *rval = INT_TO_JSVAL(nativeRet);
  }
  else if (argc == 6)
  {
     //   public int AddDirectory (  String registryName,
     //                              String version,        --OR--     VersionInfo version, 
     //                              String jarSourcePath,
     //                              Object localDirSpec,
     //                              String relativeLocalPath,
     //                              Boolean forceUpdate);  

    nsCvrtJSValToStr(b0, cx, argv[0]);
    nsCvrtJSValToStr(b2, cx, argv[2]);
    nsCvrtJSValToStr(b3, cx, argv[3]);
    nsCvrtJSValToStr(b4, cx, argv[4]);

    if(!nsCvrtJSValToBool(&b5, cx, argv[5]))
    {
      return JS_FALSE;
    }

    if(JSVAL_IS_OBJECT(argv[1]))
    {
        JSObject* jsobj = JSVAL_TO_OBJECT(argv[1]);
        JSClass* jsclass = JS_GetClass(cx, jsobj);
        if ((nsnull != jsclass) && (jsclass->flags & JSCLASS_HAS_PRIVATE)) 
        {
          nsIDOMInstallVersion* version = (nsIDOMInstallVersion*)JS_GetPrivate(cx, jsobj);

          if(NS_OK != nativeThis->AddDirectory(b0, version, b2, b3, b4, b5, &nativeRet))
          {
                return JS_FALSE;
          }
        }
    }
    else
    {
        nsCvrtJSValToStr(b1, cx, argv[1]);
        if(NS_OK != nativeThis->AddDirectory(b0, b1, b2, b3, b4, b5, &nativeRet))
        {
            return JS_FALSE;
        }
    }

    *rval = INT_TO_JSVAL(nativeRet);
  }
  else
  {
    JS_ReportError(cx, "Install.AddDirectory() parameters error");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method AddSubcomponent
//
PR_STATIC_CALLBACK(JSBool)
InstallAddSubcomponent(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsInstall *nativeThis = (nsInstall*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;
  PRInt32 nativeRet;
  nsAutoString b0;
  nsAutoString b1;
  nsAutoString b2;
  nsAutoString b3;
  nsAutoString b4;
  PRBool b5;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if(argc >= 6)
  {
    //  public int AddSubcomponent ( String registryName,
    //                               String version,        --OR--     VersionInfo version, 
    //                               String jarSourcePath,
    //                               Object localDirSpec,
    //                               String relativeLocalPath,
    //                               Boolean forceUpdate); 

    nsCvrtJSValToStr(b0, cx, argv[0]);
    nsCvrtJSValToStr(b2, cx, argv[2]);
    nsCvrtJSValToStr(b3, cx, argv[3]);
    nsCvrtJSValToStr(b4, cx, argv[4]);

    if(!nsCvrtJSValToBool(&b5, cx, argv[5]))
    {
      return JS_FALSE;
    }

    if(JSVAL_IS_OBJECT(argv[1]))
    {
      JSObject* jsobj = JSVAL_TO_OBJECT(argv[1]);
      JSClass* jsclass = JS_GetClass(cx, jsobj);
      if ((nsnull != jsclass) && (jsclass->flags & JSCLASS_HAS_PRIVATE)) 
      {
        nsIDOMInstallVersion* version = (nsIDOMInstallVersion*)JS_GetPrivate(cx, jsobj);

        if(NS_OK != nativeThis->AddSubcomponent(b0, version, b2, b3, b4, b5, &nativeRet))
        {
          return JS_FALSE;
        }
      }
    }
    else
    {
      nsCvrtJSValToStr(b1, cx, argv[1]);
      if(NS_OK != nativeThis->AddSubcomponent(b0, b1, b2, b3, b4, b5, &nativeRet))
      {
        return JS_FALSE;
      }
    }

    *rval = INT_TO_JSVAL(nativeRet);
  }
  else if(argc >= 5)
  {
    //  public int AddSubcomponent ( String registryName,
    //                               String version,  --OR-- VersionInfo version
    //                               String jarSourcePath,
    //                               Object localDirSpec,
    //                               String relativeLocalPath);

    nsCvrtJSValToStr(b0, cx, argv[0]);
    nsCvrtJSValToStr(b2, cx, argv[2]);
    nsCvrtJSValToStr(b3, cx, argv[3]);
    nsCvrtJSValToStr(b4, cx, argv[4]);

    if(JSVAL_IS_OBJECT(argv[1]))
    {
      JSObject* jsobj = JSVAL_TO_OBJECT(argv[1]);
      JSClass* jsclass = JS_GetClass(cx, jsobj);
      if ((nsnull != jsclass) && (jsclass->flags & JSCLASS_HAS_PRIVATE)) 
      {
        nsIDOMInstallVersion* version = (nsIDOMInstallVersion*)JS_GetPrivate(cx, jsobj);

        if(NS_OK != nativeThis->AddSubcomponent(b0, version, b2, b3, b4, &nativeRet))
        {
          return JS_FALSE;
        }
      }
    }
    else
    {
      nsCvrtJSValToStr(b1, cx, argv[1]);
      if(NS_OK != nativeThis->AddSubcomponent(b0, b1, b2, b3, b4, &nativeRet))
      {
        return JS_FALSE;
      }
    }

    *rval = INT_TO_JSVAL(nativeRet);
  }
  else if(argc >= 4)
  {
    //  public int AddSubcomponent ( String registryName,
    //                               String jarSourcePath,
    //                               Object localDirSpec,
    //                               String relativeLocalPath);

    nsCvrtJSValToStr(b0, cx, argv[0]);
    nsCvrtJSValToStr(b1, cx, argv[1]);
    nsCvrtJSValToStr(b2, cx, argv[2]);
    nsCvrtJSValToStr(b3, cx, argv[3]);

    if(NS_OK != nativeThis->AddSubcomponent(b0, b1, b2, b3, &nativeRet))
    {
      return JS_FALSE;
    }

    *rval = INT_TO_JSVAL(nativeRet);
  }
  else if(argc >= 1)
  {
    //  public int AddSubcomponent ( String jarSourcePath);

    nsCvrtJSValToStr(b0, cx, argv[0]);

    if(NS_OK != nativeThis->AddSubcomponent(b0, &nativeRet))
    {
      return JS_FALSE;
    }

    *rval = INT_TO_JSVAL(nativeRet);
  }
  else
  {
    JS_ReportError(cx, "Function AddSubcomponent requires 6 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method DeleteComponent
//
PR_STATIC_CALLBACK(JSBool)
InstallDeleteComponent(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsInstall *nativeThis = (nsInstall*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;
  PRInt32 nativeRet;
  nsAutoString b0;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if(argc >= 1)
  {
    //  public int DeleteComponent ( String registryName);

    nsCvrtJSValToStr(b0, cx, argv[0]);

    if(NS_OK != nativeThis->DeleteComponent(b0, &nativeRet))
    {
      return JS_FALSE;
    }

    *rval = INT_TO_JSVAL(nativeRet);
  }
  else
  {
    JS_ReportError(cx, "Function DeleteComponent requires 1 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method DeleteFile
//
PR_STATIC_CALLBACK(JSBool)
InstallDeleteFile(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsInstall *nativeThis = (nsInstall*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;
  PRInt32 nativeRet;
  nsAutoString b0;
  nsAutoString b1;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if(argc >= 2)
  {
    //  public int DeleteFile ( Object folder,
    //                          String relativeFileName);

    nsCvrtJSValToStr(b0, cx, argv[0]);
    nsCvrtJSValToStr(b1, cx, argv[1]);

    if(NS_OK != nativeThis->DeleteFile(b0, b1, &nativeRet))
    {
      return JS_FALSE;
    }

    *rval = INT_TO_JSVAL(nativeRet);
  }
  else
  {
    JS_ReportError(cx, "Function DeleteFile requires 2 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method DiskSpaceAvailable
//
PR_STATIC_CALLBACK(JSBool)
InstallDiskSpaceAvailable(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsInstall *nativeThis = (nsInstall*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;
  PRInt32 nativeRet;
  nsAutoString b0;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if(argc >= 1)
  {
    //  public int DiskSpaceAvailable ( Object localDirSpec);

    nsCvrtJSValToStr(b0, cx, argv[0]);

    if(NS_OK != nativeThis->DiskSpaceAvailable(b0, &nativeRet))
    {
      return JS_FALSE;
    }

    *rval = INT_TO_JSVAL(nativeRet);
  }
  else
  {
    JS_ReportError(cx, "Function DiskSpaceAvailable requires 1 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method Execute
//
PR_STATIC_CALLBACK(JSBool)
InstallExecute(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsInstall *nativeThis = (nsInstall*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;
  PRInt32 nativeRet;
  nsAutoString b0;
  nsAutoString b1;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if(argc >= 2)
  {
    //  public int Execute ( String jarSourcePath,
    //                       String args);

    nsCvrtJSValToStr(b0, cx, argv[0]);
    nsCvrtJSValToStr(b1, cx, argv[1]);

    if(NS_OK != nativeThis->Execute(b0, b1, &nativeRet))
    {
      return JS_FALSE;
    }

    *rval = INT_TO_JSVAL(nativeRet);
  }
  else if(argc >= 1)
  {
    //  public int Execute ( String jarSourcePath);

    nsCvrtJSValToStr(b0, cx, argv[0]);

    if(NS_OK != nativeThis->Execute(b0, &nativeRet))
    {
      return JS_FALSE;
    }

    *rval = INT_TO_JSVAL(nativeRet);
  }
  else
  {
    JS_ReportError(cx, "Function Execute requires 2 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method FinalizeInstall
//
PR_STATIC_CALLBACK(JSBool)
InstallFinalizeInstall(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsInstall *nativeThis = (nsInstall*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;
  PRInt32 nativeRet;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if(argc >= 0)
  {
    //  public int FinalizeInstall (void);

    if(NS_OK != nativeThis->FinalizeInstall(&nativeRet))
    {
      return JS_FALSE;
    }

    *rval = INT_TO_JSVAL(nativeRet);
  }
  else
  {
    JS_ReportError(cx, "Function FinalizeInstall requires 0 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method Gestalt
//
PR_STATIC_CALLBACK(JSBool)
InstallGestalt(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsInstall *nativeThis = (nsInstall*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;
  PRInt32 nativeRet;
  nsAutoString b0;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if(argc >= 1)
  {
    //  public int Gestalt ( String selector,
    //                       long   *response);

    nsCvrtJSValToStr(b0, cx, argv[0]);

    if(NS_OK != nativeThis->Gestalt(b0, &nativeRet))
    {
      return JS_FALSE;
    }

    *rval = INT_TO_JSVAL(nativeRet);
  }
  else
  {
    JS_ReportError(cx, "Function Gestalt requires 1 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method GetComponentFolder
//
PR_STATIC_CALLBACK(JSBool)
InstallGetComponentFolder(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsInstall *nativeThis = (nsInstall*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;
  nsString* nativeRet;
  nsAutoString b0;
  nsAutoString b1;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if(argc >= 2)
  {
    //  public int GetComponentFolder ( String registryName,
    //                                  String subDirectory);

    nsCvrtJSValToStr(b0, cx, argv[0]);
    nsCvrtJSValToStr(b1, cx, argv[1]);

    if(NS_OK != nativeThis->GetComponentFolder(b0, b1, &nativeRet))
    {
      return JS_FALSE;
    }

    nsCvrtStrToJSVal(*nativeRet, cx, rval);
  }
  else if(argc >= 1)
  {
    //  public int GetComponentFolder ( String registryName);

    nsCvrtJSValToStr(b0, cx, argv[0]);

    if(NS_OK != nativeThis->GetComponentFolder(b0, &nativeRet))
    {
      return JS_FALSE;
    }

    nsCvrtStrToJSVal(*nativeRet, cx, rval);
  }
  else
  {
    JS_ReportError(cx, "Function GetComponentFolder requires 2 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method GetFolder
//
PR_STATIC_CALLBACK(JSBool)
InstallGetFolder(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsInstall *nativeThis = (nsInstall*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;
  nsString* nativeRet;
  nsAutoString b0;
  nsAutoString b1;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if(argc >= 2)
  {
    //  public int GetFolder ( String folderName, --OR-- Object localDirSpec,
    //                         String subDirectory);

    nsCvrtJSValToStr(b0, cx, argv[0]);
    nsCvrtJSValToStr(b1, cx, argv[1]);

    if(NS_OK != nativeThis->GetFolder(b0, b1, &nativeRet))
    {
      return JS_FALSE;
    }

    nsCvrtStrToJSVal(*nativeRet, cx, rval);
  }
  else if(argc >= 1)
  {
    //  public int GetFolder ( String folderName);

    nsCvrtJSValToStr(b0, cx, argv[0]);

    if(NS_OK != nativeThis->GetFolder(b0, &nativeRet))
    {
      return JS_FALSE;
    }

    nsCvrtStrToJSVal(*nativeRet, cx, rval);
  }
  else
  {
    JS_ReportError(cx, "Function GetFolder requires 2 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method GetLastError
//
PR_STATIC_CALLBACK(JSBool)
InstallGetLastError(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsInstall *nativeThis = (nsInstall*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;
  PRInt32 nativeRet;


  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if(argc >= 0)
  {
    //  public int GetLastError (void);

    if(NS_OK != nativeThis->GetLastError(&nativeRet))
    {
      return JS_FALSE;
    }

    *rval = INT_TO_JSVAL(nativeRet);
  }
  else
  {
    JS_ReportError(cx, "Function GetLastError requires 0 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method GetWinProfile
//
PR_STATIC_CALLBACK(JSBool)
InstallGetWinProfile(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsInstall *nativeThis = (nsInstall*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;
  PRInt32 nativeRet;
  nsAutoString b0;
  nsAutoString b1;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if(argc >= 2)
  {
    //  public int GetWinProfile (Object folder,
    //                            String file);

    nsCvrtJSValToStr(b0, cx, argv[0]);
    nsCvrtJSValToStr(b1, cx, argv[1]);

    if(NS_OK != nativeThis->GetWinProfile(b0, b1, &nativeRet))
    {
      return JS_FALSE;
    }

    *rval = INT_TO_JSVAL(nativeRet);
  }
  else
  {
    JS_ReportError(cx, "Function GetWinProfile requires 2 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method GetWinRegistry
//
PR_STATIC_CALLBACK(JSBool)
InstallGetWinRegistry(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsInstall *nativeThis = (nsInstall*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;

  *rval = JSVAL_NULL;

#ifdef XP_WIN
  // If there's no private data, this must be the prototype, so ignore
  if(nsnull == nativeThis)
  {
    return JS_TRUE;
  }

  if(argc >= 0)
  {
    //  public int GetWinRegistry (void);
    if(NS_OK != nativeThis->GetWinRegistry(cx, &WinRegClass, rval))
    {
      return JS_FALSE;
    }
  }
  else
  {
    JS_ReportError(cx, "Function GetWinRegistry requires 0 parameters");
    return JS_FALSE;
  }
#endif

  return JS_TRUE;
}


//
// Native method Patch
//
PR_STATIC_CALLBACK(JSBool)
InstallPatch(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsInstall *nativeThis = (nsInstall*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;
  PRInt32 nativeRet;
  nsAutoString b0;
  nsAutoString b1;
  nsAutoString b2;
  nsAutoString b3;
  nsAutoString b4;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if(argc >= 5)
  {
    //  public int Patch (String registryName,
    //                    String version,        --OR-- VersionInfo version,
    //                    String jarSourcePath,
    //                    Object localDirSpec,
    //                    String relativeLocalPath);

    nsCvrtJSValToStr(b0, cx, argv[0]);
    nsCvrtJSValToStr(b2, cx, argv[2]);
    nsCvrtJSValToStr(b3, cx, argv[3]);
    nsCvrtJSValToStr(b4, cx, argv[4]);

    if(JSVAL_IS_OBJECT(argv[1]))
    {
      JSObject* jsobj = JSVAL_TO_OBJECT(argv[1]);
      JSClass* jsclass = JS_GetClass(cx, jsobj);
      if ((nsnull != jsclass) && (jsclass->flags & JSCLASS_HAS_PRIVATE)) 
      {
        nsIDOMInstallVersion* version = (nsIDOMInstallVersion*)JS_GetPrivate(cx, jsobj);

        if(NS_OK != nativeThis->Patch(b0, version, b2, b3, b4, &nativeRet))
        {
          return JS_FALSE;
        }
      }
    }
    else
    {
      nsCvrtJSValToStr(b1, cx, argv[1]);
      if(NS_OK != nativeThis->Patch(b0, b1, b2, b3, b4, &nativeRet))
      {
        return JS_FALSE;
      }
    }

    *rval = INT_TO_JSVAL(nativeRet);
  }
  else if(argc >= 4)
  {
    //  public int Patch (String registryName,
    //                    String jarSourcePath,
    //                    Object localDirSpec,
    //                    String relativeLocalPath);

    nsCvrtJSValToStr(b0, cx, argv[0]);
    nsCvrtJSValToStr(b1, cx, argv[1]);
    nsCvrtJSValToStr(b2, cx, argv[2]);
    nsCvrtJSValToStr(b3, cx, argv[3]);

    if(NS_OK != nativeThis->Patch(b0, b1, b2, b3, &nativeRet))
    {
      return JS_FALSE;
    }

    *rval = INT_TO_JSVAL(nativeRet);
  }
  else
  {
    JS_ReportError(cx, "Function Patch requires 5 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method ResetError
//
PR_STATIC_CALLBACK(JSBool)
InstallResetError(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsInstall *nativeThis = (nsInstall*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if(argc >= 0)
  {
    //  public int ResetError (void);

    if(NS_OK != nativeThis->ResetError())
    {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }
  else
  {
    JS_ReportError(cx, "Function ResetError requires 0 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method SetPackageFolder
//
PR_STATIC_CALLBACK(JSBool)
InstallSetPackageFolder(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsInstall *nativeThis = (nsInstall*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;
  nsAutoString b0;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if(argc >= 1)
  {
    //  public int SetPackageFolder (Object folder);

    nsCvrtJSValToStr(b0, cx, argv[0]);

    if(NS_OK != nativeThis->SetPackageFolder(b0))
    {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }
  else
  {
    JS_ReportError(cx, "Function SetPackageFolder requires 1 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}

//
// Native method StartInstall
//
PR_STATIC_CALLBACK(JSBool)
InstallStartInstall(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsInstall *nativeThis = (nsInstall*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;
  PRInt32 nativeRet;
  nsAutoString b0;
  nsAutoString b1;
  nsAutoString b2;
  PRInt32 b3;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if(argc >= 4)
  {
    //  public int StartInstall (String userPackageName,
    //                           String package,
    //                           String version,  --OR--  VersionInfo version,
    //                           int flags);

    nsCvrtJSValToStr(b0, cx, argv[0]);
    nsCvrtJSValToStr(b1, cx, argv[1]);

    if(!JS_ValueToInt32(cx, argv[3], (int32 *)&b3))
    {
      JS_ReportError(cx, "Parameter must be a number");
      return JS_FALSE;
    }

    if(JSVAL_IS_OBJECT(argv[1]))
    {
      JSObject* jsobj = JSVAL_TO_OBJECT(argv[2]);
      JSClass* jsclass = JS_GetClass(cx, jsobj);
      if ((nsnull != jsclass) && (jsclass->flags & JSCLASS_HAS_PRIVATE)) 
      {
        nsIDOMInstallVersion* version = (nsIDOMInstallVersion*)JS_GetPrivate(cx, jsobj);

        if(NS_OK != nativeThis->StartInstall(b0, b1, version, b3, &nativeRet))
        {
          return JS_FALSE;
        }
      }
    }
    else
    {
      nsCvrtJSValToStr(b1, cx, argv[1]);
      if(NS_OK != nativeThis->StartInstall(b0, b1, b2, b3, &nativeRet))
      {
        return JS_FALSE;
      }
    }

    *rval = INT_TO_JSVAL(nativeRet);
  }
  else if(argc >= 3)
  {
    //  public int StartInstall (String userPackageName,
    //                           String package,
    //                           String version); --OR-- VersionInfo version

    nsCvrtJSValToStr(b0, cx, argv[0]);
    nsCvrtJSValToStr(b1, cx, argv[1]);

    if(JSVAL_IS_OBJECT(argv[1]))
    {
      JSObject* jsobj = JSVAL_TO_OBJECT(argv[2]);
      JSClass* jsclass = JS_GetClass(cx, jsobj);
      if ((nsnull != jsclass) && (jsclass->flags & JSCLASS_HAS_PRIVATE)) 
      {
        nsIDOMInstallVersion* version = (nsIDOMInstallVersion*)JS_GetPrivate(cx, jsobj);

        if(NS_OK != nativeThis->StartInstall(b0, b1, version, &nativeRet))
        {
          return JS_FALSE;
        }
      }
    }
    else
    {
      nsCvrtJSValToStr(b1, cx, argv[1]);
      if(NS_OK != nativeThis->StartInstall(b0, b1, b2, &nativeRet))
      {
        return JS_FALSE;
      }
    }

    *rval = INT_TO_JSVAL(nativeRet);
  }
  else
  {
    JS_ReportError(cx, "Function StartInstall requires 4 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method Uninstall
//
PR_STATIC_CALLBACK(JSBool)
InstallUninstall(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsInstall *nativeThis = (nsInstall*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;
  PRInt32 nativeRet;
  nsAutoString b0;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if(argc >= 1)
  {
    //  public int Uninstall (String packageName);

    nsCvrtJSValToStr(b0, cx, argv[0]);

    if(NS_OK != nativeThis->Uninstall(b0, &nativeRet))
    {
      return JS_FALSE;
    }

    *rval = INT_TO_JSVAL(nativeRet);
  }
  else
  {
    JS_ReportError(cx, "Function Uninstall requires 1 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


/***********************************************************************/
//
// class for Install
//
JSClass InstallClass = {
  "Install", 
  JSCLASS_HAS_PRIVATE,
  JS_PropertyStub,
  JS_PropertyStub,
  GetInstallProperty,
  SetInstallProperty,
  JS_EnumerateStub,
  JS_ResolveStub,
  JS_ConvertStub,
  FinalizeInstall
};

//
// Install class properties
//
static JSPropertySpec InstallProperties[] =
{
  {"userPackageName",   INSTALL_USERPACKAGENAME,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"regPackageName",    INSTALL_REGPACKAGENAME,     JSPROP_ENUMERATE | JSPROP_READONLY},
  {"silent",            INSTALL_SILENT,             JSPROP_ENUMERATE | JSPROP_READONLY},
  {"force",             INSTALL_FORCE,              JSPROP_ENUMERATE | JSPROP_READONLY},
  {"jarfile",           INSTALL_JARFILE,            JSPROP_ENUMERATE | JSPROP_READONLY},
  {"arguments",         INSTALL_ARGUMENTS,          JSPROP_ENUMERATE | JSPROP_READONLY},
  {0}
};


static JSConstDoubleSpec install_constants[] = 
{
    { nsInstall::BAD_PACKAGE_NAME,           "BAD_PACKAGE_NAME"              },
    { nsInstall::UNEXPECTED_ERROR,           "UNEXPECTED_ERROR"              },
    { nsInstall::ACCESS_DENIED,              "ACCESS_DENIED"                },
    { nsInstall::NO_INSTALLER_CERTIFICATE,   "NO_INSTALLER_CERTIFICATE"     },
    { nsInstall::NO_CERTIFICATE,             "NO_CERTIFICATE"               },
    { nsInstall::NO_MATCHING_CERTIFICATE,    "NO_MATCHING_CERTIFICATE"      },
    { nsInstall::UNKNOWN_JAR_FILE,           "UNKNOWN_JAR_FILE"             },
    { nsInstall::INVALID_ARGUMENTS,          "INVALID_ARGUMENTS"            },
    { nsInstall::ILLEGAL_RELATIVE_PATH,      "ILLEGAL_RELATIVE_PATH"        },
    { nsInstall::USER_CANCELLED,             "USER_CANCELLED"               },
    { nsInstall::INSTALL_NOT_STARTED,        "INSTALL_NOT_STARTED"          },
    { nsInstall::SILENT_MODE_DENIED,         "SILENT_MODE_DENIED"           },
    { nsInstall::FILE_DOES_NOT_EXIST,        "FILE_DOES_NOT_EXIST"          },
    { nsInstall::FILE_READ_ONLY,             "FILE_READ_ONLY"               },
    { nsInstall::FILE_IS_DIRECTORY,          "FILE_IS_DIRECTORY"            },
    { nsInstall::NETWORK_FILE_IS_IN_USE,     "NETWORK_FILE_IS_IN_USE"       },
    { nsInstall::APPLE_SINGLE_ERR,           "APPLE_SINGLE_ERR"             },
    { nsInstall::INVALID_PATH_ERR,           "INVALID_PATH_ERR"             },
    { nsInstall::PATCH_BAD_DIFF,             "PATCH_BAD_DIFF"               },
    { nsInstall::PATCH_BAD_CHECKSUM_TARGET,  "PATCH_BAD_CHECKSUM_TARGET"    },
    { nsInstall::PATCH_BAD_CHECKSUM_RESULT,  "PATCH_BAD_CHECKSUM_RESULT"    },
    { nsInstall::UNINSTALL_FAILED,           "UNINSTALL_FAILED"             },
    { nsInstall::GESTALT_UNKNOWN_ERR,        "GESTALT_UNKNOWN_ERR"          },
    { nsInstall::GESTALT_INVALID_ARGUMENT,   "GESTALT_INVALID_ARGUMENT"     },
    { nsInstall::SUCCESS,                    "SUCCESS"                      },
    { nsInstall::REBOOT_NEEDED,              "REBOOT_NEEDED"                },
    { nsInstall::LIMITED_INSTALL,            "LIMITED_INSTALL"              },
    { nsInstall::FULL_INSTALL,               "FULL_INSTALL"                 },
    { nsInstall::NO_STATUS_DLG ,             "NO_STATUS_DLG"                },
    { nsInstall::NO_FINALIZE_DLG,            "NO_FINALIZE_DLG"              },
    {0}
};

//
// Install class methods
//
static JSFunctionSpec InstallMethods[] = 
{
  {"AbortInstall",              InstallAbortInstall,            0},
  {"AddDirectory",              InstallAddDirectory,            6},
  {"AddSubcomponent",           InstallAddSubcomponent,         6},
  {"DeleteComponent",           InstallDeleteComponent,         1},
  {"DeleteFile",                InstallDeleteFile,              2},
  {"DiskSpaceAvailable",        InstallDiskSpaceAvailable,      1},
  {"Execute",                   InstallExecute,                 2},
  {"FinalizeInstall",           InstallFinalizeInstall,         0},
  {"Gestalt",                   InstallGestalt,                 1},
  {"GetComponentFolder",        InstallGetComponentFolder,      2},
  {"GetFolder",                 InstallGetFolder,               2},
  {"GetLastError",              InstallGetLastError,            0},
  {"GetWinProfile",             InstallGetWinProfile,           2},
  {"GetWinRegistry",            InstallGetWinRegistry,          0},
  {"Patch",                     InstallPatch,                   5},
  {"ResetError",                InstallResetError,              0},
  {"SetPackageFolder",          InstallSetPackageFolder,        1},
  {"StartInstall",              InstallStartInstall,            4},
  {"Uninstall",                 InstallUninstall,               1},
  {0}
};


//
// WinProfile class methods
//
// static JSFunctionSpec WinProfileMethods[] = 
// {
//   {"writeString",                WinProfileWriteString,               3},
//   {"getString",                  WinProfileGetString,                 2},
//   {"getFilename",                WinProfileGetFilename,               0},
//   {"install",                    WinProfileInstall,                   0},
//   {"finalWriteString",           WinProfileFinalWriteString,          3},
//   {0}
// };


//
// Install constructor
//
PR_STATIC_CALLBACK(JSBool)
Install(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  return JS_FALSE;
}

//
// Install class initialization
//

PRInt32 InitXPInstallObjects(nsIScriptContext *aContext, const char* jarfile, const char* args)
{
  JSContext *jscontext  = (JSContext *)aContext->GetNativeContext();
  JSObject *global      = JS_GetGlobalObject(jscontext);
  JSObject *installObject = nsnull;
  JSObject *winRegPrototype = nsnull;
//   JSObject *winProfilePrototype = nsnull;
  nsInstall *nativeInstallObject;

  installObject  = JS_InitClass( jscontext,         // context
                                 global,            // global object
                                 nsnull,            // parent proto 
                                 &InstallClass,     // JSClass
                                 nsnull,            // JSNative ctor
                                 0,                 // ctor args
                                 nsnull,            // proto props
                                 nsnull,            // proto funcs
                                 InstallProperties, // ctor props (static)
                                 InstallMethods);   // ctor funcs (static)

//   winProfilePrototype  = JS_InitClass( jscontext,          // context
//                                        global,             // global object
//                                        nsnull,             // parent proto 
//                                        &WinProfileClass,   // JSClass
//                                        nsnull,             // JSNative ctor
//                                        0,                  // ctor args
//                                        nsnull,             // proto props
//                                        nsnull,             // proto funcs
//                                        nsnull,             // ctor props (static)
//                                        WinProfileMethods); // ctor funcs (static)

  if (nsnull == installObject) 
  {
      return NS_ERROR_FAILURE;
  }

  if ( PR_FALSE == JS_DefineConstDoubles(jscontext, installObject, install_constants) )
            return NS_ERROR_FAILURE;
  
  nativeInstallObject = new nsInstall();

  nativeInstallObject->SetJarFileLocation(jarfile);
  nativeInstallObject->SetInstallArguments(args);

  JS_SetPrivate(jscontext, installObject, nativeInstallObject);
  nativeInstallObject->SetScriptObject(installObject);
 
#ifdef XP_WIN
  if(NS_OK != InitWinRegPrototype(jscontext, global, &winRegPrototype))
  {
      return NS_ERROR_FAILURE;
  }
  nativeInstallObject->SaveWinRegPrototype(winRegPrototype);

//   if(NS_OK != InitWinProfileObject(jscontext, global, winRegPrototype)
//   {
//       return NS_ERROR_FAILURE;
//   }
//   nativeInstallObject->SaveWinProfilePrototype(winProfilePrototype);
#endif

  return NS_OK;
}




PRInt32 InitXPInstallObjects(JSContext *jscontext, JSObject *global, const char* jarfile, const char* args)
{
  JSObject *installObject   = nsnull;
  JSObject *winRegPrototype = nsnull;
//   JSObject *winProfilePrototype = nsnull;
  nsInstall *nativeInstallObject;

  installObject  = JS_InitClass( jscontext,         // context
                                 global,            // global object
                                 nsnull,            // parent proto 
                                 &InstallClass,     // JSClass
                                 nsnull,            // JSNative ctor
                                 0,                 // ctor args
                                 nsnull,            // proto props
                                 nsnull,            // proto funcs
                                 InstallProperties, // ctor props (static)
                                 InstallMethods);   // ctor funcs (static)

//   winProfilePrototype  = JS_InitClass( jscontext,          // context
//                                        global,             // global object
//                                        nsnull,             // parent proto 
//                                        &WinProfileClass,   // JSClass
//                                        nsnull,             // JSNative ctor
//                                        0,                  // ctor args
//                                        nsnull,             // proto props
//                                        nsnull,             // proto funcs
//                                        nsnull,             // ctor props (static)
//                                        WinProfileMethods); // ctor funcs (static)

  if (nsnull == installObject) 
  {
      return NS_ERROR_FAILURE;
  }

  if ( PR_FALSE == JS_DefineConstDoubles(jscontext, installObject, install_constants) )
            return NS_ERROR_FAILURE;
  
  nativeInstallObject = new nsInstall();

  nativeInstallObject->SetJarFileLocation(jarfile);
  nativeInstallObject->SetInstallArguments(args);

  JS_SetPrivate(jscontext, installObject, nativeInstallObject);
  nativeInstallObject->SetScriptObject(installObject);
 
#ifdef XP_WIN
  if(NS_OK != InitWinRegPrototype(jscontext, global, &winRegPrototype))
  {
      return NS_ERROR_FAILURE;
  }
  nativeInstallObject->SaveWinRegPrototype(winRegPrototype);

//   if(NS_OK != InitWinProfileObject(jscontext, global, winRegPrototype)
//   {
//       return NS_ERROR_FAILURE;
//   }
//   nativeInstallObject->SaveWinProfilePrototype(winProfilePrototype);
#endif
 
  return NS_OK;
}
