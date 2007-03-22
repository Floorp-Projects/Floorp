/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * ***** BEGIN LICENSE BLOCK *****
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
 * Sun Microsystems, Inc.
 * Portions created by the Initial Developer are Copyright (C) 1999
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
#ifndef JNIEnvTests_h___
#define JNIEnvTests_h___

#ifdef XP_WIN
#include <windows.h>
#endif
#include "nsIServiceManager.h"
#include "nsIJVMManager.h"
#include "nsJVMManager.h"

//#include "jni.h"

#include "ojiapitests.h"

#define SecENV TRUE

static NS_DEFINE_CID(kJVMManagerCID,NS_JVMMANAGER_CID);
static NS_DEFINE_IID(kIJVMManagerIID, NS_IJVMMANAGER_IID);

extern nsresult GetJNI(JNIEnv **env);

typedef unsigned char byte;

#ifdef XP_UNIX
#define MAX_JLONG jlong_MAXINT  // from jri_md.h
#define MIN_JLONG jlong_MININT
#else
#define MAX_JLONG  9223372036854775807
#define MIN_JLONG -9223372036854775808
#endif

#define MAX_JINT  2147483647
#define MIN_JINT -2147483648
#define MAX_JDOUBLE  1.7976931348623157E308
#define MIN_JDOUBLE  4.9E-324
#define MAX_JBYTE  127
#define MIN_JBYTE -128
#define MAX_JFLOAT  3.4028235E38F
#define MIN_JFLOAT  1.4E-45F
#define MAX_JSHORT  32767
#define MIN_JSHORT -32768


#endif //JNIEnvTests_h___
