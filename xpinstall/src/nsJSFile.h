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

#ifndef __NS_JSFILE_H__
#define __NS_JSFILE_H__

#include "jsapi.h"
#include "nscore.h"


JSBool PR_CALLBACK
InstallFileOpDirCreate(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);

JSBool PR_CALLBACK
InstallFileOpDirGetParent(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);

JSBool PR_CALLBACK
InstallFileOpDirRemove(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);

JSBool PR_CALLBACK
InstallFileOpDirRename(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);

JSBool PR_CALLBACK
InstallFileOpFileCopy(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);

JSBool PR_CALLBACK
InstallFileOpFileRemove(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);

JSBool PR_CALLBACK
InstallFileOpFileExists(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);

JSBool PR_CALLBACK
InstallFileOpFileExecute(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);

JSBool PR_CALLBACK
InstallFileOpFileGetNativeVersion(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);

JSBool PR_CALLBACK
InstallFileOpFileGetDiskSpaceAvailable(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);

JSBool PR_CALLBACK
InstallFileOpFileGetModDate(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);

JSBool PR_CALLBACK
InstallFileOpFileGetSize(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);

JSBool PR_CALLBACK
InstallFileOpFileIsDirectory(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);

JSBool PR_CALLBACK
InstallFileOpFileIsWritable(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);

JSBool PR_CALLBACK
InstallFileOpFileIsFile(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);

JSBool PR_CALLBACK
InstallFileOpFileModDateChanged(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);

JSBool PR_CALLBACK
InstallFileOpFileMove(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);

JSBool PR_CALLBACK
InstallFileOpFileRename(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);

JSBool PR_CALLBACK
InstallFileOpFileWindowsShortcut(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);

JSBool PR_CALLBACK
InstallFileOpFileMacAlias(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);

JSBool PR_CALLBACK
InstallFileOpFileUnixLink(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);

PRInt32 InitXPFileOpObjectPrototype(JSContext *jscontext, JSObject *global, JSObject **fileOpObjectPrototype);

#endif
