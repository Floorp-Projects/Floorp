/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "jsapi.h"
#include "nsJSUtils.h"
#include "nscore.h"
#include "nsIScriptContext.h"

#include "nsString.h"
#include "nsInstall.h"

#include "nsIDOMInstallVersion.h"

#include "stdio.h"

#ifdef _WINDOWS
#include "nsJSWinReg.h"
#include "nsJSWinProfile.h"

extern JSClass WinRegClass;
extern JSClass WinProfileClass;
#endif

#include "nsJSFileSpecObj.h"
extern JSClass FileSpecObjectClass;


/* externs from nsJSInstall.cpp */
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



//
// Native method DirCreate
//
JSBool PR_CALLBACK
InstallFileOpDirCreate(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsInstall *nativeThis = (nsInstall*)JS_GetPrivate(cx, obj);
  PRInt32 nativeRet;
  JSObject *jsObj;
  nsInstallFolder *folder;

  *rval = INT_TO_JSVAL(nsInstall::UNEXPECTED_ERROR);

  // If there's no private data, this must be the prototype, so ignore
  if(nsnull == nativeThis)
  {
    return JS_TRUE;
  }

  //  public int DirCreate (nsInstallFolder aNativeFolderPath);

  if ( argc == 0 || argv[0] == JSVAL_NULL || !JSVAL_IS_OBJECT(argv[0])) //argv[0] MUST be a jsval
  {
    *rval = INT_TO_JSVAL(nsInstall::INVALID_ARGUMENTS);
    return JS_TRUE;
  }

  jsObj = JSVAL_TO_OBJECT(argv[0]);

  if (!JS_InstanceOf(cx, jsObj, &FileSpecObjectClass, nsnull))
  {
    *rval = INT_TO_JSVAL(nsInstall::INVALID_ARGUMENTS);
    return JS_TRUE;
  }

  folder = (nsInstallFolder*)JS_GetPrivate(cx, jsObj);
  
  if(!folder || NS_OK != nativeThis->FileOpDirCreate(*folder, &nativeRet))
  {
    return JS_TRUE;
  }

  *rval = INT_TO_JSVAL(nativeRet);
  return JS_TRUE;
}


//
// Native method DirGetParent
//
JSBool PR_CALLBACK
InstallFileOpDirGetParent(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsInstall*   nativeThis = (nsInstall*)JS_GetPrivate(cx, obj);
  nsCOMPtr<nsIFile>   nativeRet;
  nsString     nativeRetNSStr;
  JSObject *jsObj;
  nsInstallFolder *folder;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if(nsnull == nativeThis)
  {
    return JS_TRUE;
  }


  //  public int DirGetParent (nsInstallFolder NativeFolderPath);

  if ( argc == 0 || argv[0] == JSVAL_NULL || !JSVAL_IS_OBJECT(argv[0])) //argv[0] MUST be a jsval
  {
    // error, return NULL
    return JS_TRUE;
  }

  jsObj = JSVAL_TO_OBJECT(argv[0]);

  if (!JS_InstanceOf(cx, jsObj, &FileSpecObjectClass, nsnull))
  {
    // error, return NULL
    return JS_TRUE;
  }

  folder = (nsInstallFolder*)JS_GetPrivate(cx, jsObj);
  
  if(!folder || NS_OK != nativeThis->FileOpDirGetParent(*folder, getter_AddRefs(nativeRet)))
  {
    // error, return NULL
    return JS_TRUE;
  }

  char* temp;
  nativeRet->GetPath(&temp);
  nativeRetNSStr.AssignWithConversion(temp);

  *rval = STRING_TO_JSVAL(JS_NewUCStringCopyN(cx, nativeRetNSStr.GetUnicode(), nativeRetNSStr.Length()));

  return JS_TRUE;
}

//
// Native method DirRemove
//
JSBool PR_CALLBACK
InstallFileOpDirRemove(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsInstall *nativeThis = (nsInstall*)JS_GetPrivate(cx, obj);
  PRInt32 nativeRet;
  PRBool  bRecursive = PR_FALSE;
  JSObject *jsObj;
  nsInstallFolder *folder;

  *rval = INT_TO_JSVAL(nsInstall::UNEXPECTED_ERROR);

  // If there's no private data, this must be the prototype, so ignore
  if(nsnull == nativeThis)
  {
    return JS_TRUE;
  }

    //  public int DirRemove (nsInstallFolder aNativeFolderPath,
    //                        Bool   aRecursive);

  if ( argc == 0 || argv[0] == JSVAL_NULL || !JSVAL_IS_OBJECT(argv[0])) //argv[0] MUST be a jsval
  {
    *rval = INT_TO_JSVAL(nsInstall::INVALID_ARGUMENTS);
    return JS_TRUE;
  }

  jsObj = JSVAL_TO_OBJECT(argv[0]);

  if (!JS_InstanceOf(cx, jsObj, &FileSpecObjectClass, nsnull))
  {
    *rval = INT_TO_JSVAL(nsInstall::INVALID_ARGUMENTS);
    return JS_TRUE;
  }

  folder = (nsInstallFolder*)JS_GetPrivate(cx, jsObj);

  if( argc > 1 && !ConvertJSValToBool(&bRecursive, cx, argv[1]))
  {
    JS_ReportError(cx, "2nd parameter needs to be a Boolean value");
    return JS_TRUE;
  }

  if(!folder || NS_OK != nativeThis->FileOpDirRemove(*folder, bRecursive, &nativeRet))
  {
    return JS_TRUE;
  }

  *rval = INT_TO_JSVAL(nativeRet);

  return JS_TRUE;
}

//
// Native method DirRename
//
JSBool PR_CALLBACK
InstallFileOpDirRename(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsInstall *nativeThis = (nsInstall*)JS_GetPrivate(cx, obj);
  PRInt32 nativeRet;
  nsAutoString b1;
  JSObject *jsObj;
  nsInstallFolder *folder;

  *rval = INT_TO_JSVAL(nsInstall::UNEXPECTED_ERROR);

  // If there's no private data, this must be the prototype, so ignore
  if(nsnull == nativeThis)
  {
    return JS_TRUE;
  }

  if(argc >= 2)
  {
    //  public int DirRename (String aSourceFolder,
    //                        String aTargetFolder);

    ConvertJSValToStr(b1, cx, argv[1]);

// fix: nsFileSpec::Rename() does not accept new name as a
//      nsFileSpec type.  It only accepts a char* type for the new name
//      This is a bug with nsFileSpec.  A char* will be used until
//      nsFileSpec if fixed.
//    nsFileSpec fsB1(b1);
    
    if (argv[0] == JSVAL_NULL || !JSVAL_IS_OBJECT(argv[0])) //argv[0] MUST be a jsval
    {
      *rval = INT_TO_JSVAL(nsInstall::INVALID_ARGUMENTS);
      return JS_TRUE;
    }

    jsObj = JSVAL_TO_OBJECT(argv[0]);

    if (!JS_InstanceOf(cx, jsObj, &FileSpecObjectClass, nsnull))
    {
      *rval = INT_TO_JSVAL(nsInstall::INVALID_ARGUMENTS);
      return JS_TRUE;
    }

    folder = (nsInstallFolder*)JS_GetPrivate(cx, jsObj);

    if(!folder || NS_OK != nativeThis->FileOpDirRename(*folder, b1, &nativeRet))
    {
      return JS_TRUE;
    }

    *rval = INT_TO_JSVAL(nativeRet);
  }
  else
  {
    JS_ReportError(cx, "Function DirRename requires 2 parameters");
    return JS_TRUE;
  }

  return JS_TRUE;
}

//
// Native method FileCopy
//
JSBool PR_CALLBACK
InstallFileOpFileCopy(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsInstall *nativeThis = (nsInstall*)JS_GetPrivate(cx, obj);
  PRInt32 nativeRet;
  JSObject *jsSrcObj, *jsTargetObj;
  nsInstallFolder *srcFolder, *targetFolder;

  *rval = INT_TO_JSVAL(nsInstall::UNEXPECTED_ERROR);

  // If there's no private data, this must be the prototype, so ignore
  if(nsnull == nativeThis)
  {
    return JS_TRUE;
  }

  if(argc >= 2)
  {
    //  public int FileCopy (nsInstallFolder aSourceFolder,
    //                       nsInstallFolder aTargetFolder);

    if (argv[0] == JSVAL_NULL || !JSVAL_IS_OBJECT(argv[0])) //argv[0] MUST be a jsval
    {
      *rval = INT_TO_JSVAL(nsInstall::INVALID_ARGUMENTS);
      return JS_TRUE;
    }

    if (argv[1] == JSVAL_NULL || !JSVAL_IS_OBJECT(argv[1])) //argv[1] MUST be a jsval
    {
      *rval = INT_TO_JSVAL(nsInstall::INVALID_ARGUMENTS);
      return JS_TRUE;
    }

    jsSrcObj = JSVAL_TO_OBJECT(argv[0]);
    jsTargetObj = JSVAL_TO_OBJECT(argv[1]);

    if (!JS_InstanceOf(cx, jsSrcObj, &FileSpecObjectClass, nsnull))
    {
      *rval = INT_TO_JSVAL(nsInstall::INVALID_ARGUMENTS);
      return JS_TRUE;
    }
    if (!JS_InstanceOf(cx, jsTargetObj, &FileSpecObjectClass, nsnull))
    {
      *rval = INT_TO_JSVAL(nsInstall::INVALID_ARGUMENTS);
      return JS_TRUE;
    }

    srcFolder = (nsInstallFolder*)JS_GetPrivate(cx, jsSrcObj);
    targetFolder = (nsInstallFolder*)JS_GetPrivate(cx, jsTargetObj);
    PRInt32 tempRet;
    tempRet = nativeThis->FileOpFileCopy(*srcFolder, *targetFolder, &nativeRet);

    if(!srcFolder || !targetFolder || NS_OK != tempRet)
    {
      return JS_TRUE;
    }

    *rval = INT_TO_JSVAL(nativeRet);
  }
  else
  {
    JS_ReportError(cx, "Function FileCopy requires 2 parameters");
    return JS_TRUE;
  }

  return JS_TRUE;
}

//
// Native method FileDelete
//
JSBool PR_CALLBACK
InstallFileOpFileRemove(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsInstall *nativeThis = (nsInstall*)JS_GetPrivate(cx, obj);
  PRInt32 nativeRet;
  JSObject *jsObj;
  nsInstallFolder *folder;

  *rval = INT_TO_JSVAL(nsInstall::UNEXPECTED_ERROR);

  // If there's no private data, this must be the prototype, so ignore
  if(nsnull == nativeThis)
  {
    return JS_TRUE;
  }

    //  public int FileDelete (nsInstallFolder aSourceFolder);

  if (argc == 0 || argv[0] == JSVAL_NULL || !JSVAL_IS_OBJECT(argv[0])) //argv[0] MUST be a jsval
  {
    *rval = INT_TO_JSVAL(nsInstall::INVALID_ARGUMENTS);
    return JS_TRUE;
  }

  jsObj = JSVAL_TO_OBJECT(argv[0]);

  if (!JS_InstanceOf(cx, jsObj, &FileSpecObjectClass, nsnull))
  {
    *rval = INT_TO_JSVAL(nsInstall::INVALID_ARGUMENTS);
    return JS_TRUE;
  }

  folder = (nsInstallFolder*)JS_GetPrivate(cx, jsObj);

  if(!folder || NS_OK != nativeThis->FileOpFileDelete(*folder, PR_FALSE, &nativeRet))
  {
    return JS_TRUE;
  }

  *rval = INT_TO_JSVAL(nativeRet);

  return JS_TRUE;
}

//
// Native method FileExists
//
JSBool PR_CALLBACK
InstallFileOpFileExists(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsInstall *nativeThis = (nsInstall*)JS_GetPrivate(cx, obj);
  PRInt32 nativeRet;
  JSObject *jsObj;
  nsInstallFolder *folder;

  *rval = INT_TO_JSVAL(nsInstall::UNEXPECTED_ERROR);

  // If there's no private data, this must be the prototype, so ignore
  if(nsnull == nativeThis)
  {
    return JS_TRUE;
  }

  //  public int FileExists (nsInstallFolder NativeFolderPath);

  if (argc == 0 || argv[0] == JSVAL_NULL || !JSVAL_IS_OBJECT(argv[0])) //argv[0] MUST be a jsval
  {
    *rval = INT_TO_JSVAL(nsInstall::INVALID_ARGUMENTS);
    return JS_TRUE;
  }

  jsObj = JSVAL_TO_OBJECT(argv[0]);

  if (!JS_InstanceOf(cx, jsObj, &FileSpecObjectClass, nsnull))
  {
    *rval = INT_TO_JSVAL(nsInstall::INVALID_ARGUMENTS);
    return JS_TRUE;
  }

  folder = (nsInstallFolder*)JS_GetPrivate(cx, jsObj);

  if(!folder || NS_OK != nativeThis->FileOpFileExists(*folder, &nativeRet))
  {
    return JS_TRUE;
  }

  *rval = INT_TO_JSVAL(nativeRet);

  return JS_TRUE;
}

//
// Native method FileExecute
//
JSBool PR_CALLBACK
InstallFileOpFileExecute(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsInstall *nativeThis = (nsInstall*)JS_GetPrivate(cx, obj);
  PRInt32 nativeRet;
  nsAutoString b1;
  JSObject *jsObj;
  nsInstallFolder *folder;

  *rval = INT_TO_JSVAL(nsInstall::UNEXPECTED_ERROR);

  // If there's no private data, this must be the prototype, so ignore
  if(nsnull == nativeThis)
  {
    return JS_TRUE;
  }

  if(argc >= 2)
  {
    //  public int FileExecute (nsInstallFolder aSourceFolder,
    //                          String aParameters);

    ConvertJSValToStr(b1, cx, argv[1]);
  }
  else
    b1.SetLength(0);

  if (argv[0] == JSVAL_NULL || !JSVAL_IS_OBJECT(argv[0])) //argv[0] MUST be a jsval
  {
      *rval = INT_TO_JSVAL(nsInstall::INVALID_ARGUMENTS);
      return JS_TRUE;
  }

  jsObj = JSVAL_TO_OBJECT(argv[0]);

  if (!JS_InstanceOf(cx, jsObj, &FileSpecObjectClass, nsnull))
  {
      *rval = INT_TO_JSVAL(nsInstall::INVALID_ARGUMENTS);
      return JS_TRUE;
  }

  folder = (nsInstallFolder*)JS_GetPrivate(cx, jsObj);

  if(NS_OK != nativeThis->FileOpFileExecute(*folder, b1, &nativeRet))
  {
     return JS_TRUE;
  }

  *rval = INT_TO_JSVAL(nativeRet);

  return JS_TRUE;
}

//
// Native method FileGetNativeVersion
//
JSBool PR_CALLBACK
InstallFileOpFileGetNativeVersion(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsInstall*   nativeThis = (nsInstall*)JS_GetPrivate(cx, obj);
  nsAutoString nativeRet;
  JSObject *jsObj;
  nsInstallFolder *folder;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if(nsnull == nativeThis)
  {
    return JS_TRUE;
  }

  //  public int FileGetNativeVersion (nsInstallFolder NativeFolderPath);

  if (argc == 0 || argv[0] == JSVAL_NULL || !JSVAL_IS_OBJECT(argv[0])) //argv[0] MUST be a jsval
  {
    *rval = JSVAL_NULL;
    return JS_TRUE;
  }

  jsObj = JSVAL_TO_OBJECT(argv[0]);

  if (!JS_InstanceOf(cx, jsObj, &FileSpecObjectClass, nsnull))
  {
    *rval = JSVAL_NULL;
    return JS_TRUE;
  }

  folder = (nsInstallFolder*)JS_GetPrivate(cx, jsObj);

  if(!folder || NS_OK != nativeThis->FileOpFileGetNativeVersion(*folder, &nativeRet))
  {
    return JS_TRUE;
  }

  *rval = STRING_TO_JSVAL(JS_NewUCStringCopyN(cx, nativeRet.GetUnicode(), nativeRet.Length()));

  return JS_TRUE;
}

//
// Native method FileGetDiskSpaceAvailable
//
JSBool PR_CALLBACK
InstallFileOpFileGetDiskSpaceAvailable(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{

  nsInstall*   nativeThis = (nsInstall*)JS_GetPrivate(cx, obj);
  PRInt64     nativeRet;
  JSObject *jsObj;
  nsInstallFolder *folder;

  *rval = INT_TO_JSVAL(nsInstall::UNEXPECTED_ERROR);

  // If there's no private data, this must be the prototype, so ignore
  if(nsnull == nativeThis)
  {
    return JS_TRUE;
  }

  //  public int FileGetDiskSpaceAvailable (String NativeFolderPath);

  if (argc == 0 || argv[0] == JSVAL_NULL || !JSVAL_IS_OBJECT(argv[0])) //argv[0] MUST be a jsval
  {
    *rval = INT_TO_JSVAL(nsInstall::INVALID_ARGUMENTS);
    return JS_TRUE;
  }

  jsObj = JSVAL_TO_OBJECT(argv[0]);

  if (!JS_InstanceOf(cx, jsObj, &FileSpecObjectClass, nsnull))
  {
    *rval = INT_TO_JSVAL(nsInstall::INVALID_ARGUMENTS);
    return JS_TRUE;
  }

  folder = (nsInstallFolder*)JS_GetPrivate(cx, jsObj);

  if(!folder || NS_OK != nativeThis->FileOpFileGetDiskSpaceAvailable(*folder, &nativeRet))
  {
    return JS_TRUE;
  }
  
  double d;
  LL_L2D(d, nativeRet);
  JS_NewDoubleValue( cx, d, rval );

  return JS_TRUE;
}

//
// Native method FileGetModDate
//
JSBool PR_CALLBACK
InstallFileOpFileGetModDate(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsInstall*   nativeThis = (nsInstall*)JS_GetPrivate(cx, obj);
  PRUint32     nativeRet;
  JSObject *jsObj;
  nsInstallFolder *folder;

  *rval = INT_TO_JSVAL(nsInstall::UNEXPECTED_ERROR);;

  // If there's no private data, this must be the prototype, so ignore
  if(nsnull == nativeThis)
  {
    return JS_TRUE;
  }

  //  public int FileGetModDate (nsInstallFolder NativeFolderPath);

  if (argc == 0 || argv[0] == JSVAL_NULL || !JSVAL_IS_OBJECT(argv[0])) //argv[0] MUST be a jsval
  {
    *rval = INT_TO_JSVAL(nsInstall::INVALID_ARGUMENTS);
    return JS_TRUE;
  }

  jsObj = JSVAL_TO_OBJECT(argv[0]);

  if (!JS_InstanceOf(cx, jsObj, &FileSpecObjectClass, nsnull))
  {
    *rval = INT_TO_JSVAL(nsInstall::INVALID_ARGUMENTS);
    return JS_TRUE;
  }

  folder = (nsInstallFolder*)JS_GetPrivate(cx, jsObj);

  if(!folder || NS_OK != nativeThis->FileOpFileGetModDate(*folder, &nativeRet))
  {
    return JS_TRUE;
  }

  if ( nativeRet <= JSVAL_INT_MAX )
    *rval = INT_TO_JSVAL(nativeRet);
  else
  {
    JSInt64 l;
    jsdouble d;

    JSLL_UI2L( l, nativeRet );
    JSLL_L2D( d, l );

    JS_NewDoubleValue( cx, d, rval );
  }

  return JS_TRUE;
}

//
// Native method FileGetSize
//
JSBool PR_CALLBACK
InstallFileOpFileGetSize(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsInstall*   nativeThis = (nsInstall*)JS_GetPrivate(cx, obj);
  PRInt64     nativeRet;
  JSObject *jsObj;
  nsInstallFolder *folder;

  *rval = INT_TO_JSVAL(nsInstall::UNEXPECTED_ERROR);

  // If there's no private data, this must be the prototype, so ignore
  if(nsnull == nativeThis)
  {
    return JS_TRUE;
  }

  //  public int FileGetSize (String NativeFolderPath);

  if (argc == 0 || argv[0] == JSVAL_NULL || !JSVAL_IS_OBJECT(argv[0])) //argv[0] MUST be a jsval
  {
    *rval = INT_TO_JSVAL(nsInstall::INVALID_ARGUMENTS);
    return JS_TRUE;
  }

  jsObj = JSVAL_TO_OBJECT(argv[0]);

  if (!JS_InstanceOf(cx, jsObj, &FileSpecObjectClass, nsnull))
  {
    *rval = INT_TO_JSVAL(nsInstall::INVALID_ARGUMENTS);
    return JS_TRUE;
  }

  folder = (nsInstallFolder*)JS_GetPrivate(cx, jsObj);

  if(!folder || NS_OK != nativeThis->FileOpFileGetSize(*folder, &nativeRet))
  {
    return JS_TRUE;
  }
   
  PRFloat64 f; /* jsdouble's *are* PRFloat64's */
  
  LL_L2F( f, nativeRet ); /* make float which is same type for js and nspr (native double) */
  JS_NewDoubleValue( cx, f, rval );
  
  return JS_TRUE;
}

//
// Native method FileIsDirectory
//
JSBool PR_CALLBACK
InstallFileOpFileIsDirectory(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsInstall *nativeThis = (nsInstall*)JS_GetPrivate(cx, obj);
  PRInt32 nativeRet;
  JSObject *jsObj;
  nsInstallFolder *folder;

  *rval = BOOLEAN_TO_JSVAL(PR_FALSE);

  // If there's no private data, this must be the prototype, so ignore
  if(nsnull == nativeThis)
  {
    return JS_TRUE;
  }

  //  public int FileIsDirectory (String NativeFolderPath);

  if (argc == 0 || argv[0] == JSVAL_NULL || !JSVAL_IS_OBJECT(argv[0])) //argv[0] MUST be a jsval
  {
    *rval = BOOLEAN_TO_JSVAL(PR_FALSE);
    return JS_TRUE;
  }

  jsObj = JSVAL_TO_OBJECT(argv[0]);

  if (!JS_InstanceOf(cx, jsObj, &FileSpecObjectClass, nsnull))
  {
    *rval = BOOLEAN_TO_JSVAL(PR_FALSE);
    return JS_TRUE;
  }

  folder = (nsInstallFolder*)JS_GetPrivate(cx, jsObj);

  if(!folder || NS_OK != nativeThis->FileOpFileIsDirectory(*folder, &nativeRet))
  {
    return JS_TRUE;
  }

  *rval = BOOLEAN_TO_JSVAL(nativeRet);

  return JS_TRUE;
}

//
// Native method FileIsFile
//
JSBool PR_CALLBACK
InstallFileOpFileIsFile(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsInstall *nativeThis = (nsInstall*)JS_GetPrivate(cx, obj);
  PRInt32 nativeRet;
  JSObject *jsObj;
  nsInstallFolder *folder;

 *rval = BOOLEAN_TO_JSVAL(PR_FALSE);

  // If there's no private data, this must be the prototype, so ignore
  if(nsnull == nativeThis)
  {
    return JS_TRUE;
  }

    //  public int FileIsFile (nsInstallFolder NativeFolderPath);

  if (argc == 0 || argv[0] == JSVAL_NULL || !JSVAL_IS_OBJECT(argv[0])) //argv[0] MUST be a jsval
  {
    *rval = BOOLEAN_TO_JSVAL(PR_FALSE);
    return JS_TRUE;
  }

  jsObj = JSVAL_TO_OBJECT(argv[0]);

  if (!JS_InstanceOf(cx, jsObj, &FileSpecObjectClass, nsnull))
  {
    *rval = BOOLEAN_TO_JSVAL(PR_FALSE);
    return JS_TRUE;
  }

  folder = (nsInstallFolder*)JS_GetPrivate(cx, jsObj);

  if(!folder || NS_OK != nativeThis->FileOpFileIsFile(*folder, &nativeRet))
  {
    return JS_TRUE;
  }

  *rval = BOOLEAN_TO_JSVAL(nativeRet);

  return JS_TRUE;
}

//
// Native method FileModDateChanged
//
JSBool PR_CALLBACK
InstallFileOpFileModDateChanged(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsInstall*   nativeThis = (nsInstall*)JS_GetPrivate(cx, obj);
  PRInt32      nativeRet;
  PRUint32     b1;
  JSObject *jsObj;
  nsInstallFolder *folder;

  *rval = BOOLEAN_TO_JSVAL(PR_FALSE);

  // If there's no private data, this must be the prototype, so ignore
  if(nsnull == nativeThis)
  {
    return JS_TRUE;
  }

  if(argc >= 2)
  {
    //  public int FileModDateChanged (nsInstallFolder aSourceFolder,
    //                                 Number aOldDate);

    b1 = JSVAL_TO_INT(argv[1]);
    if (argv[0] == JSVAL_NULL || !JSVAL_IS_OBJECT(argv[0])) //argv[0] MUST be a jsval
    {
      *rval =     *rval = BOOLEAN_TO_JSVAL(PR_FALSE);
      return JS_TRUE;
    }

    jsObj = JSVAL_TO_OBJECT(argv[0]);

    if (!JS_InstanceOf(cx, jsObj, &FileSpecObjectClass, nsnull))
    {
      *rval = BOOLEAN_TO_JSVAL(PR_FALSE);
      return JS_TRUE;
    }

    folder = (nsInstallFolder*)JS_GetPrivate(cx, jsObj);

    if(!folder || NS_OK != nativeThis->FileOpFileModDateChanged(*folder, b1, &nativeRet))
    {
      return JS_TRUE;
    }

    *rval = BOOLEAN_TO_JSVAL(nativeRet);
  }
  else
  {
    JS_ReportError(cx, "Function FileModDateChanged requires 2 parameters");
    return JS_TRUE;
  }

  return JS_TRUE;
}

//
// Native method FileMove
//
JSBool PR_CALLBACK
InstallFileOpFileMove(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsInstall *nativeThis = (nsInstall*)JS_GetPrivate(cx, obj);
  PRInt32 nativeRet;
  JSObject *jsSrcObj, *jsTargetObj;
  nsInstallFolder *srcFolder, *targetFolder;

  *rval = INT_TO_JSVAL(nsInstall::UNEXPECTED_ERROR);

  // If there's no private data, this must be the prototype, so ignore
  if(nsnull == nativeThis)
  {
    return JS_TRUE;
  }

  if(argc >= 2)
  {
    //  public int FileMove (nsInstallFolder aSourceFolder,
    //                       nsInstallFolder aTargetFolder);

    if (argv[0] == JSVAL_NULL || !JSVAL_IS_OBJECT(argv[0])) //argv[0] MUST be a jsval
    {
      *rval = INT_TO_JSVAL(nsInstall::INVALID_ARGUMENTS);
      return JS_TRUE;
    }
    if (argv[1] == JSVAL_NULL || !JSVAL_IS_OBJECT(argv[1])) //argv[0] MUST be a jsval
    {
      *rval = INT_TO_JSVAL(nsInstall::INVALID_ARGUMENTS);
      return JS_TRUE;
    }

    jsSrcObj = JSVAL_TO_OBJECT(argv[0]);
    jsTargetObj = JSVAL_TO_OBJECT(argv[1]);

    if (!JS_InstanceOf(cx, jsSrcObj, &FileSpecObjectClass, nsnull))
    {
      *rval = INT_TO_JSVAL(nsInstall::INVALID_ARGUMENTS);
      return JS_TRUE;
    }
    if (!JS_InstanceOf(cx, jsTargetObj, &FileSpecObjectClass, nsnull))
    {
      *rval = INT_TO_JSVAL(nsInstall::INVALID_ARGUMENTS);
      return JS_TRUE;
    }

    srcFolder = (nsInstallFolder*)JS_GetPrivate(cx, jsSrcObj);
    targetFolder = (nsInstallFolder*)JS_GetPrivate(cx, jsTargetObj);

    if(!srcFolder || !targetFolder || NS_OK != nativeThis->FileOpFileMove(*srcFolder, *targetFolder, &nativeRet))
    {
      return JS_TRUE;
    }

    *rval = INT_TO_JSVAL(nativeRet);
  }
  else
  {
    JS_ReportError(cx, "Function FileMove requires 2 parameters");
    return JS_TRUE;
  }

  return JS_TRUE;
}

//
// Native method FileRename
//
JSBool PR_CALLBACK
InstallFileOpFileRename(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsInstall *nativeThis = (nsInstall*)JS_GetPrivate(cx, obj);
  PRInt32 nativeRet;
  nsAutoString b1;
  JSObject *jsObj;
  nsInstallFolder *folder;

  *rval = INT_TO_JSVAL(nsInstall::UNEXPECTED_ERROR);

  // If there's no private data, this must be the prototype, so ignore
  if(nsnull == nativeThis)
  {
    return JS_TRUE;
  }

  if(argc >= 2)
  {
    //  public int FileRename (nsInstallFolder aSourceFolder,
    //                         String aTargetFolder);

    ConvertJSValToStr(b1, cx, argv[1]);

// fix: nsFileSpec::Rename() does not accept new name as a
//      nsFileSpec type.  It only accepts a char* type for the new name
//      This is a bug with nsFileSpec.  A char* will be used until
//      nsFileSpec if fixed.
//    nsFileSpec fsB1(b1);

    if (argv[0] == JSVAL_NULL || !JSVAL_IS_OBJECT(argv[0])) //argv[0] MUST be a jsval
    {
      *rval = INT_TO_JSVAL(nsInstall::INVALID_ARGUMENTS);
      return JS_TRUE;
    }

    jsObj = JSVAL_TO_OBJECT(argv[0]);

    if (!JS_InstanceOf(cx, jsObj, &FileSpecObjectClass, nsnull))
    {
      *rval = INT_TO_JSVAL(nsInstall::INVALID_ARGUMENTS);
      return JS_TRUE;
    }

    folder = (nsInstallFolder*)JS_GetPrivate(cx, jsObj);

    if(!folder || NS_OK != nativeThis->FileOpFileRename(*folder, b1, &nativeRet))
    {
      return JS_TRUE;
    }

    *rval = INT_TO_JSVAL(nativeRet);
  }
  else
  {
    JS_ReportError(cx, "Function FileRename requires 2 parameters");
    return JS_TRUE;
  }

  return JS_TRUE;
}

//
// Native method FileWindowsShortcut
//
JSBool PR_CALLBACK
InstallFileOpFileWindowsShortcut(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsInstall *nativeThis = (nsInstall*)JS_GetPrivate(cx, obj);
  PRInt32 nativeRet;
  nsAutoString b0;
  nsAutoString b1;
  nsAutoString b2;
  nsAutoString b3;
  nsAutoString b4;
  nsAutoString b5;
  nsCOMPtr<nsILocalFile> nsfsB0;
  nsCOMPtr<nsILocalFile> nsfsB1;
  nsCOMPtr<nsILocalFile> nsfsB3;
  nsCOMPtr<nsILocalFile> nsfsB5;
  PRInt32      b6;

  //JSObject *jsObj;
  //nsInstallFolder *folder;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if(nsnull == nativeThis)
  {
    return JS_TRUE;
  }

  if(argc >= 7)
  {
    //  public int FileWindowsShortcut(String aTarget,
    //                                 String aShortcutPath,
    //                                 String aDescription,
    //                                 String aWorkingPath,
    //                                 String aParams,
    //                                 String aIcon,
    //                                 Number aIconId);


    ConvertJSValToStr(b0, cx, argv[0]);
    NS_NewLocalFile(nsAutoCString(b0), getter_AddRefs(nsfsB0));
    ConvertJSValToStr(b1, cx, argv[1]);
    NS_NewLocalFile(nsAutoCString(b1), getter_AddRefs(nsfsB1));
    ConvertJSValToStr(b2, cx, argv[2]);
    ConvertJSValToStr(b3, cx, argv[3]);
    NS_NewLocalFile(nsAutoCString(b3), getter_AddRefs(nsfsB3));
    ConvertJSValToStr(b4, cx, argv[4]);
    ConvertJSValToStr(b5, cx, argv[5]);
    NS_NewLocalFile(nsAutoCString(b5), getter_AddRefs(nsfsB5));

    if(JSVAL_IS_NULL(argv[6]))
    {
      b6 = 0;
    }
    else
    {
      b6 = JSVAL_TO_INT(argv[6]);
    }

    if(NS_OK != nativeThis->FileOpFileWindowsShortcut(nsfsB0, nsfsB1, b2, nsfsB3, b4, nsfsB5, b6, &nativeRet))
    {
      return JS_TRUE;
    }

    *rval = INT_TO_JSVAL(nativeRet);
  }
  else
  {
    JS_ReportError(cx, "Function FileWindowsShortcut requires 7 parameters");
    return JS_TRUE;
  }

  return JS_TRUE;
}

//
// Native method FileMacAlias
//
JSBool PR_CALLBACK
InstallFileOpFileMacAlias(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsInstall *nativeThis = (nsInstall*)JS_GetPrivate(cx, obj);
  PRInt32 nativeRet;
  nsAutoString b0;
  nsAutoString b1;
  nsAutoString b2;
  nsAutoString b3;
  //JSObject *jsObj;
  //nsInstallFolder *folder;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if(nsnull == nativeThis)
  {
    return JS_TRUE;
  }

  if(argc >= 4)
  {
    // public int FileMacAlias( String aSourceFolder,
    //                          String aSourceFileName,
    //                          String aAliasFolder,
    //                          String aAliasName );
    // where
    //      aSourceFolder -- the folder of the installed file from Get*Folder() APIs
    //      aSourceName   -- the installed file's name
    //      aAliasFolder  -- the folder of the new alias from Get*Folder() APIs
    //      aAliasName    -- the actual name of the new alias
    //      
    //      returns SUCCESS for successful scheduling of operation
    //              UNEXPECTED_ERROR for failure
    
    ConvertJSValToStr(b0, cx, argv[0]);
    ConvertJSValToStr(b1, cx, argv[1]);
    ConvertJSValToStr(b2, cx, argv[2]);
    ConvertJSValToStr(b3, cx, argv[3]);

    b0 += b1;
    b2 += b3;
	
    if(NS_OK != nativeThis->FileOpFileMacAlias(b0, b2, &nativeRet))
    {
      return JS_TRUE;
    }

    *rval = INT_TO_JSVAL(nativeRet);
  }
  else if (argc >= 3)
  {
    // public int FileMacAlias( String aSourceFolder,
    //                          String aSourceName,
    //                          String aAliasFolder );
    // where
    //      aSourceFolder -- the folder of the installed file from Get*Folder() APIs
    //      aSourceName   -- the installed file's name
    //      aAliasFolder  -- the folder of the new alias from Get*Folder() APIs
    //
    // NOTE: 
    //      The destination alias name is aSourceName + " alias" (Mac-like behavior). 
    //      
    //      returns SUCCESS for successful scheduling of operation
    //              UNEXPECTED_ERROR for failure
    
    ConvertJSValToStr(b0, cx, argv[0]);
    ConvertJSValToStr(b1, cx, argv[1]);
    ConvertJSValToStr(b2, cx, argv[2]);
    
    b0 += b1;
    b2 += b1;
    b2.AppendWithConversion(" alias");   // XXX use GetResourcedString(id)
    
    if(NS_OK != nativeThis->FileOpFileMacAlias(b0, b2, &nativeRet))
    {
      return JS_TRUE;
    }

    *rval = INT_TO_JSVAL(nativeRet);
  }
  else if (argc >= 2)
  {
    // public int FileMacAlias( String aSourcePath,
    //                          String aAliasPath );
    // where
    //      aSourcePath -- the full path to the installed file 
    //      aAliasPath  -- the full path to the new alias
    //      
    //      returns SUCCESS for successful scheduling of operation
    //              UNEXPECTED_ERROR for failure
    
    ConvertJSValToStr(b0, cx, argv[0]);
    ConvertJSValToStr(b1, cx, argv[1]);
    
    if(NS_OK != nativeThis->FileOpFileMacAlias(b0, b1, &nativeRet))
    {
      return JS_TRUE;
    }

    *rval = INT_TO_JSVAL(nativeRet);
  }
  else
  {
    JS_ReportError(cx, "Function FileMacAlias requires 2 to 4 parameters");
    return JS_TRUE;
  }

  return JS_TRUE;
}

//
// Native method FileUnixLinkCreate
//
JSBool PR_CALLBACK
InstallFileOpFileUnixLink(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsInstall *nativeThis = (nsInstall*)JS_GetPrivate(cx, obj);
  PRInt32 nativeRet;
  //nsAutoString b0;
  PRInt32      b1;
  JSObject *jsObj;
  nsInstallFolder *folder;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if(nsnull == nativeThis)
  {
    return JS_TRUE;
  }

  if(argc >= 2)
  {
    //  public int FileUnixLinkCreate (String aSourceFolder,
    //                                 Number aFlags);

    b1 = JSVAL_TO_INT(argv[1]);
    if (argv[0] == JSVAL_NULL || !JSVAL_IS_OBJECT(argv[0])) //argv[0] MUST be a jsval
    {
      *rval = INT_TO_JSVAL(nsInstall::INVALID_ARGUMENTS);
      return JS_TRUE;
    }

    jsObj = JSVAL_TO_OBJECT(argv[0]);

    if (!JS_InstanceOf(cx, jsObj, &FileSpecObjectClass, nsnull))
    {
      *rval = INT_TO_JSVAL(nsInstall::INVALID_ARGUMENTS);
      return JS_TRUE;
    }

    folder = (nsInstallFolder*)JS_GetPrivate(cx, jsObj);

    if(NS_OK != nativeThis->FileOpFileUnixLink(*folder, b1, &nativeRet))
    {
      return JS_TRUE;
    }

    *rval = INT_TO_JSVAL(nativeRet);
  }
  else
  {
    JS_ReportError(cx, "Function FileUnixLink requires 2 parameters");
    return JS_TRUE;
  }

  return JS_TRUE;
}

/***********************************************************************/
//
// Install Properties Getter
//
JSBool PR_CALLBACK
GetFileProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  return PR_TRUE;
}

/***********************************************************************/
//
// File Properties Setter
//
JSBool PR_CALLBACK
SetFileProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{

  return PR_TRUE;
}



/***********************************************************************/
//
// class for FileOp operations
//
JSClass FileOpClass = {
  "File", 
  JSCLASS_HAS_PRIVATE,
  JS_PropertyStub,
  JS_PropertyStub,
  GetFileProperty,
  SetFileProperty,
  JS_EnumerateStub,
  JS_ResolveStub,
  JS_ConvertStub,
  JS_FinalizeStub
};


//
// File class methods
//
static JSFunctionSpec FileOpMethods[] = 
{
  {"dirCreate",                 InstallFileOpDirCreate,                1},
  {"dirGetParent",              InstallFileOpDirGetParent,             1},
  {"dirRemove",                 InstallFileOpDirRemove,                2},
  {"dirRename",                 InstallFileOpDirRename,                2},
  {"copy",                      InstallFileOpFileCopy,                 2},
  {"remove",                    InstallFileOpFileRemove,               1},
  {"exists",                    InstallFileOpFileExists,               1},
  {"execute",                   InstallFileOpFileExecute,              2},
  {"nativeVersion",             InstallFileOpFileGetNativeVersion,     1},
  {"diskSpaceAvailable",        InstallFileOpFileGetDiskSpaceAvailable,1},
  {"modDate",                   InstallFileOpFileGetModDate,           1},
  {"size",                      InstallFileOpFileGetSize,              1},
  {"isDirectory",               InstallFileOpFileIsDirectory,          1},
  {"isFile",                    InstallFileOpFileIsFile,               1},
  {"modDateChanged",            InstallFileOpFileModDateChanged,       2},
  {"move",                      InstallFileOpFileMove,                 2},
  {"rename",                    InstallFileOpFileRename,               2},
  {"windowsShortcut",           InstallFileOpFileWindowsShortcut,      7},
  {"macAlias",                  InstallFileOpFileMacAlias,             2},
  {"unixLink",                  InstallFileOpFileUnixLink,             2},
  {0}
};



PRInt32 InitXPFileOpObjectPrototype(JSContext *jscontext, 
                                    JSObject *global,
                                    JSObject **fileOpObjectPrototype)

{
    if (global == nsnull)
    {
        return NS_ERROR_FAILURE;
    }

    *fileOpObjectPrototype  =  JS_InitClass( jscontext,  // context
                                 global,            // global object
                                 nsnull,            // parent proto 
                                 &FileOpClass,      // JSClass
                                 nsnull,            // JSNative ctor
                                 0,                 // ctor args
                                 nsnull,            // proto props
                                 nsnull,            // proto funcs
                                 nsnull,            // ctor props (static)
                                 FileOpMethods);    // ctor funcs (static)

  if (nsnull == fileOpObjectPrototype) 
  {
      return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

