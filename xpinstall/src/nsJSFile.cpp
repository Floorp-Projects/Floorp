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
  nsAutoString b0;

  *rval = INT_TO_JSVAL(nsInstall::UNEXPECTED_ERROR);

  // If there's no private data, this must be the prototype, so ignore
  if(nsnull == nativeThis)
  {
    return JS_TRUE;
  }

  if(argc >= 1)
  {
    //  public int DirCreate (String aNativeFolderPath);

    ConvertJSValToStr(b0, cx, argv[0]);
    nsFileSpec fsB0(b0);

    if(NS_OK != nativeThis->FileOpDirCreate(fsB0, &nativeRet))
    {
      return JS_FALSE;
    }

    *rval = INT_TO_JSVAL(nativeRet);
  }
  else
  {
    JS_ReportError(cx, "Function DirCreate requires 1 parameter");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method DirGetParent
//
JSBool PR_CALLBACK
InstallFileOpDirGetParent(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsInstall*   nativeThis = (nsInstall*)JS_GetPrivate(cx, obj);
  nsFileSpec   nativeRet;
  nsAutoString b0;
  nsString     nativeRetNSStr;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if(nsnull == nativeThis)
  {
    return JS_TRUE;
  }

  if(argc >= 1)
  {
    //  public int DirGetParent (String NativeFolderPath);

    ConvertJSValToStr(b0, cx, argv[0]);
    nsFileSpec fsB0(b0);

    if(NS_OK != nativeThis->FileOpDirGetParent(fsB0, &nativeRet))
    {
      return JS_FALSE;
    }

    nativeRetNSStr = nativeRet.GetNativePathCString();
    *rval = STRING_TO_JSVAL(JS_NewUCStringCopyN(cx, nativeRetNSStr.GetUnicode(), nativeRetNSStr.Length()));
  }
  else
  {
    JS_ReportError(cx, "Function DirGetParent requires 1 parameter");
    return JS_FALSE;
  }

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
  nsAutoString b0;
  PRBool       b1;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if(nsnull == nativeThis)
  {
    return JS_TRUE;
  }

  if(argc >= 2)
  {
    //  public int DirRemove (String aNativeFolderPath,
    //                        Bool   aRecursive);

    ConvertJSValToStr(b0, cx, argv[0]);
    nsFileSpec fsB0(b0);
    if(!ConvertJSValToBool(&b1, cx, argv[1]))
    {
      JS_ReportError(cx, "2nd parameter needs to be a Boolean value");
      return JS_FALSE;
    }

    if(NS_OK != nativeThis->FileOpDirRemove(fsB0, b1, &nativeRet))
    {
      return JS_FALSE;
    }

    *rval = INT_TO_JSVAL(nativeRet);
  }
  else
  {
    JS_ReportError(cx, "Function DirRemove requires 2 parameters");
    return JS_FALSE;
  }

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
    //  public int DirRename (String aSourceFolder,
    //                        String aTargetFolder);

    ConvertJSValToStr(b0, cx, argv[0]);
    ConvertJSValToStr(b1, cx, argv[1]);
    nsFileSpec fsB0(b0);

// fix: nsFileSpec::Rename() does not accept new name as a
//      nsFileSpec type.  It only accepts a char* type for the new name
//      This is a bug with nsFileSpec.  A char* will be used until
//      nsFileSpec if fixed.
//    nsFileSpec fsB1(b1);

    if(NS_OK != nativeThis->FileOpDirRename(fsB0, b1, &nativeRet))
    {
      return JS_FALSE;
    }

    *rval = INT_TO_JSVAL(nativeRet);
 }
  else
  {
    JS_ReportError(cx, "Function DirRename requires 2 parameters");
    return JS_FALSE;
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
    //  public int FileCopy (String aSourceFolder,
    //                       String aTargetFolder);

    ConvertJSValToStr(b0, cx, argv[0]);
    ConvertJSValToStr(b1, cx, argv[1]);
    nsFileSpec fsB0(b0);
    nsFileSpec fsB1(b1);

    if(NS_OK != nativeThis->FileOpFileCopy(fsB0, fsB1, &nativeRet))
    {
      return JS_FALSE;
    }

    *rval = INT_TO_JSVAL(nativeRet);
  }
  else
  {
    JS_ReportError(cx, "Function FileCopy requires 2 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}

//
// Native method FileDelete
//
JSBool PR_CALLBACK
InstallFileOpFileDelete(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsInstall *nativeThis = (nsInstall*)JS_GetPrivate(cx, obj);
  PRInt32 nativeRet;
  nsAutoString b0;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if(nsnull == nativeThis)
  {
    return JS_TRUE;
  }

  if(argc >= 1)
  {
    //  public int FileDelete (String aSourceFolder);

    ConvertJSValToStr(b0, cx, argv[0]);
    nsFileSpec fsB0(b0);

    if(NS_OK != nativeThis->FileOpFileDelete(fsB0, PR_FALSE, &nativeRet))
    {
      return JS_FALSE;
    }

    *rval = INT_TO_JSVAL(nativeRet);
  }
  else
  {
    JS_ReportError(cx, "Function FileDelete requires 1 parameter");
    return JS_FALSE;
  }

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
  nsAutoString b0;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if(nsnull == nativeThis)
  {
    return JS_TRUE;
  }

  if(argc >= 1)
  {
    //  public int FileExists (String NativeFolderPath);

    ConvertJSValToStr(b0, cx, argv[0]);
    nsFileSpec fsB0(b0);

    if(NS_OK != nativeThis->FileOpFileExists(fsB0, &nativeRet))
    {
      return JS_FALSE;
    }

    *rval = INT_TO_JSVAL(nativeRet);
  }
  else
  {
    JS_ReportError(cx, "Function FileExists requires 1 parameter");
    return JS_FALSE;
  }

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
    //  public int FileExecute (String aSourceFolder,
    //                          String aParameters);

    ConvertJSValToStr(b0, cx, argv[0]);
    ConvertJSValToStr(b1, cx, argv[1]);
    nsFileSpec fsB0(b0);

    if(NS_OK != nativeThis->FileOpFileExecute(fsB0, b1, &nativeRet))
    {
      return JS_FALSE;
    }

    *rval = INT_TO_JSVAL(nativeRet);
  }
  else if(argc == 1)
  {
    //  public int FileExecute (String aSourceFolder,
    //                          String aParameters);

    ConvertJSValToStr(b0, cx, argv[0]);
    b1 = "";
    nsFileSpec fsB0(b0);

    if(NS_OK != nativeThis->FileOpFileExecute(fsB0, b1, &nativeRet))
    {
      return JS_FALSE;
    }

    *rval = INT_TO_JSVAL(nativeRet);
  }
  else
  {
    JS_ReportError(cx, "Function FileExecute requires 1 or 2 parameters");
    return JS_FALSE;
  }

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
  nsAutoString b0;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if(nsnull == nativeThis)
  {
    return JS_TRUE;
  }

  if(argc >= 1)
  {
    //  public int FileGetNativeVersion (String NativeFolderPath);

    ConvertJSValToStr(b0, cx, argv[0]);
    nsFileSpec fsB0(b0);

    if(NS_OK != nativeThis->FileOpFileGetNativeVersion(fsB0, &nativeRet))
    {
      return JS_FALSE;
    }

    *rval = STRING_TO_JSVAL(JS_NewUCStringCopyN(cx, nativeRet.GetUnicode(), nativeRet.Length()));
  }
  else
  {
    JS_ReportError(cx, "Function FileGetNativeVersion requires 1 parameter");
    return JS_FALSE;
  }

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
  nsAutoString b0;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if(nsnull == nativeThis)
  {
    return JS_TRUE;
  }

  if(argc >= 1)
  {
    //  public int FileGetDiskSpaceAvailable (String NativeFolderPath);

    ConvertJSValToStr(b0, cx, argv[0]);
    nsFileSpec fsB0(b0);

    if(NS_OK != nativeThis->FileOpFileGetDiskSpaceAvailable(fsB0, &nativeRet))
    {
      return JS_FALSE;
    }
    
    double d;
    LL_L2D(d, nativeRet);
    JS_NewDoubleValue( cx, d, rval );
  }
  else
  {
    JS_ReportError(cx, "Function FileGetDiskSpaceAvailable requires 1 parameter");
    return JS_FALSE;
  }

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
  nsAutoString b0;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if(nsnull == nativeThis)
  {
    return JS_TRUE;
  }

  if(argc >= 1)
  {
    //  public int FileGetModDate (String NativeFolderPath);

    ConvertJSValToStr(b0, cx, argv[0]);
    nsFileSpec fsB0(b0);

    if(NS_OK != nativeThis->FileOpFileGetModDate(fsB0, &nativeRet))
    {
      return JS_FALSE;
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
  }
  else
  {
    JS_ReportError(cx, "Function FileGetModDate requires 1 parameter");
    return JS_FALSE;
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
  PRUint32     nativeRet;
  nsAutoString b0;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if(nsnull == nativeThis)
  {
    return JS_TRUE;
  }

  if(argc >= 1)
  {
    //  public int FileGetSize (String NativeFolderPath);

    ConvertJSValToStr(b0, cx, argv[0]);
    nsFileSpec fsB0(b0);

    if(NS_OK != nativeThis->FileOpFileGetSize(fsB0, &nativeRet))
    {
      return JS_FALSE;
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
  }
  else
  {
    JS_ReportError(cx, "Function FileGetSize requires 1 parameter");
    return JS_FALSE;
  }

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
  nsAutoString b0;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if(nsnull == nativeThis)
  {
    return JS_TRUE;
  }

  if(argc >= 1)
  {
    //  public int FileIsDirectory (String NativeFolderPath);

    ConvertJSValToStr(b0, cx, argv[0]);
    nsFileSpec fsB0(b0);

    if(NS_OK != nativeThis->FileOpFileIsDirectory(fsB0, &nativeRet))
    {
      return JS_FALSE;
    }

    *rval = INT_TO_JSVAL(nativeRet);
  }
  else
  {
    JS_ReportError(cx, "Function FileIsDirectory requires 1 parameter");
    return JS_FALSE;
  }

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
  nsAutoString b0;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if(nsnull == nativeThis)
  {
    return JS_TRUE;
  }

  if(argc >= 1)
  {
    //  public int FileIsFile (String NativeFolderPath);

    ConvertJSValToStr(b0, cx, argv[0]);
    nsFileSpec fsB0(b0);

    if(NS_OK != nativeThis->FileOpFileIsFile(fsB0, &nativeRet))
    {
      return JS_FALSE;
    }

    *rval = INT_TO_JSVAL(nativeRet);
  }
  else
  {
    JS_ReportError(cx, "Function FileIsFile requires 1 parameter");
    return JS_FALSE;
  }

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
  nsAutoString b0;
  PRUint32     b1;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if(nsnull == nativeThis)
  {
    return JS_TRUE;
  }

  if(argc >= 2)
  {
    //  public int FileModDateChanged (String aSourceFolder,
    //                                 Number aOldDate);

    ConvertJSValToStr(b0, cx, argv[0]);
    b1 = JSVAL_TO_INT(argv[1]);
    nsFileSpec fsB0(b0);

    if(NS_OK != nativeThis->FileOpFileModDateChanged(fsB0, b1, &nativeRet))
    {
      return JS_FALSE;
    }

    *rval = INT_TO_JSVAL(nativeRet);
  }
  else
  {
    JS_ReportError(cx, "Function FileModDateChanged requires 2 parameters");
    return JS_FALSE;
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
    //  public int FileMove (String aSourceFolder,
    //                       String aTargetFolder);

    ConvertJSValToStr(b0, cx, argv[0]);
    ConvertJSValToStr(b1, cx, argv[1]);
    nsFileSpec fsB0(b0);
    nsFileSpec fsB1(b1);

    if(NS_OK != nativeThis->FileOpFileMove(fsB0, fsB1, &nativeRet))
    {
      return JS_FALSE;
    }

    *rval = INT_TO_JSVAL(nativeRet);
  }
  else
  {
    JS_ReportError(cx, "Function FileMove requires 2 parameters");
    return JS_FALSE;
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
    //  public int FileRename (String aSourceFolder,
    //                         String aTargetFolder);

    ConvertJSValToStr(b0, cx, argv[0]);
    ConvertJSValToStr(b1, cx, argv[1]);
    nsFileSpec fsB0(b0);

// fix: nsFileSpec::Rename() does not accept new name as a
//      nsFileSpec type.  It only accepts a char* type for the new name
//      This is a bug with nsFileSpec.  A char* will be used until
//      nsFileSpec if fixed.
//    nsFileSpec fsB1(b1);

    if(NS_OK != nativeThis->FileOpFileRename(fsB0, b1, &nativeRet))
    {
      return JS_FALSE;
    }

    *rval = INT_TO_JSVAL(nativeRet);
  }
  else
  {
    JS_ReportError(cx, "Function FileRename requires 2 parameters");
    return JS_FALSE;
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
  nsFileSpec   nsfsB0;
  nsFileSpec   nsfsB1;
  nsFileSpec   nsfsB3;
  nsFileSpec   nsfsB5;
  PRInt32      b6;

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
    nsfsB0 = b0;
    ConvertJSValToStr(b1, cx, argv[1]);
    nsfsB1 = b1;
    ConvertJSValToStr(b2, cx, argv[2]);
    ConvertJSValToStr(b3, cx, argv[3]);
    nsfsB3 = b3;
    ConvertJSValToStr(b4, cx, argv[4]);
    ConvertJSValToStr(b5, cx, argv[5]);
    nsfsB5 = b5;

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
      return JS_FALSE;
    }

    *rval = INT_TO_JSVAL(nativeRet);
  }
  else
  {
    JS_ReportError(cx, "Function FileWindowsShortcut requires 7 parameters");
    return JS_FALSE;
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
      return JS_FALSE;
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
    b2 += " alias";   // XXX use GetResourcedString(id)
    
    if(NS_OK != nativeThis->FileOpFileMacAlias(b0, b2, &nativeRet))
    {
      return JS_FALSE;
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
      return JS_FALSE;
    }

    *rval = INT_TO_JSVAL(nativeRet);
  }
  else
  {
    JS_ReportError(cx, "Function FileMacAlias requires 2 to 4 parameters");
    return JS_FALSE;
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
  nsAutoString b0;
  PRInt32      b1;

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

    ConvertJSValToStr(b0, cx, argv[0]);
    b1 = JSVAL_TO_INT(argv[1]);
    nsFileSpec fsB0(b0);

    if(NS_OK != nativeThis->FileOpFileUnixLink(fsB0, b1, &nativeRet))
    {
      return JS_FALSE;
    }

    *rval = INT_TO_JSVAL(nativeRet);
  }
  else
  {
    JS_ReportError(cx, "Function FileUnixLink requires 2 parameters");
    return JS_FALSE;
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
  "FileOp", 
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
// File class properties
//
static JSPropertySpec FileProperties[] =
{
  {0}
};


static JSConstDoubleSpec file_constants[] = 
{
    {0}
};


//
// File class methods
//
static JSFunctionSpec FileOpMethods[] = 
{
  {"DirCreate",                 InstallFileOpDirCreate,                1},
  {"DirGetParent",              InstallFileOpDirGetParent,             1},
  {"DirRemove",                 InstallFileOpDirRemove,                2},
  {"DirRename",                 InstallFileOpDirRename,                2},
  {"FileCopy",                  InstallFileOpFileCopy,                 2},
  {"FileDelete",                InstallFileOpFileDelete,               2},
  {"FileExists",                InstallFileOpFileExists,               1},
  {"FileExecute",               InstallFileOpFileExecute,              2},
  {"FileGetNativeVersion",      InstallFileOpFileGetNativeVersion,     1},
  {"FileGetDiskSpaceAvailable", InstallFileOpFileGetDiskSpaceAvailable,1},
  {"FileGetModDate",            InstallFileOpFileGetModDate,           1},
  {"FileGetSize",               InstallFileOpFileGetSize,              1},
  {"FileIsDirectory",           InstallFileOpFileIsDirectory,          1},
  {"FileIsFile",                InstallFileOpFileIsFile,               1},
  {"FileModDateChanged",        InstallFileOpFileModDateChanged,       2},
  {"FileMove",                  InstallFileOpFileMove,                 2},
  {"FileRename",                InstallFileOpFileRename,               2},
  {"FileWindowsShortcut",       InstallFileOpFileWindowsShortcut,      7},
  {"FileMacAlias",              InstallFileOpFileMacAlias,             2},
  {"FileUnixLink",              InstallFileOpFileUnixLink,             2},
  {"dirCreate",                 InstallFileOpDirCreate,                1},
  {"dirGetParent",              InstallFileOpDirGetParent,             1},
  {"dirRemove",                 InstallFileOpDirRemove,                2},
  {"dirRename",                 InstallFileOpDirRename,                2},
  {"fileCopy",                  InstallFileOpFileCopy,                 2},
  {"fileDelete",                InstallFileOpFileDelete,               2},
  {"fileExists",                InstallFileOpFileExists,               1},
  {"fileExecute",               InstallFileOpFileExecute,              2},
  {"fileGetNativeVersion",      InstallFileOpFileGetNativeVersion,     1},
  {"fileGetDiskSpaceAvailable", InstallFileOpFileGetDiskSpaceAvailable,1},
  {"fileGetModDate",            InstallFileOpFileGetModDate,           1},
  {"fileGetSize",               InstallFileOpFileGetSize,              1},
  {"fileIsDirectory",           InstallFileOpFileIsDirectory,          1},
  {"fileIsFile",                InstallFileOpFileIsFile,               1},
  {"fileModDateChanged",        InstallFileOpFileModDateChanged,       2},
  {"fileMove",                  InstallFileOpFileMove,                 2},
  {"fileRename",                InstallFileOpFileRename,               2},
  {"fileWindowsShortcut",       InstallFileOpFileWindowsShortcut,      7},
  {"fileMacAlias",              InstallFileOpFileMacAlias,             2},
  {"fileUnixLink",              InstallFileOpFileUnixLink,             2},
  {0}
};



PRInt32 InitXPFileOpObjectPrototype(JSContext *jscontext, 
                                    JSObject *global,
                                    JSObject **fileOpObjectPrototype)

{
    JSObject *fileObject       = nsnull;


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

