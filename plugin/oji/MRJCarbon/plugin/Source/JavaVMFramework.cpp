/* ----- BEGIN LICENSE BLOCK -----
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
 * The Original Code is the MRJ Carbon OJI Plugin.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corp.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):  Patrick C. Beard <beard@netscape.com>
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
 * ----- END LICENSE BLOCK ----- */

/*
    JavaVMFramework.cpp
    
    CFM glue for the JavaVM.framework.
    
    by Patrick C. Beard.
 */

#include <jni.h>
#include <JavaControl.h>

#include <CFURL.h>
#include <CFBundle.h>
#include <CFString.h>
#include <MacErrors.h>

static CFBundleRef getBundle(CFStringRef frameworkPath)
{
	CFBundleRef bundle = NULL;
    
	//	Make a CFURLRef from the CFString representation of the bundle's path.
	//	See the Core Foundation URL Services chapter for details.
	CFURLRef bundleURL = CFURLCreateWithFileSystemPath(NULL, frameworkPath, kCFURLPOSIXPathStyle, true);
	if (bundleURL != NULL) {
        bundle = CFBundleCreate(NULL, bundleURL);
        if (bundle != NULL)
        	CFBundleLoadExecutable(bundle);
        CFRelease(bundleURL);
	}
	
	return bundle;
}

static void* getJavaVMFunction(CFStringRef functionName)
{
  static CFBundleRef javaBundle = getBundle(CFSTR("/System/Library/Frameworks/JavaVM.framework"));
  if (javaBundle) return CFBundleGetFunctionPointerForName(javaBundle, functionName);
  return NULL;
}

static void* getJavaEmbeddingLibFunction(CFStringRef functionName)
{
  static CFBundleRef javaBundle = getBundle(CFSTR("/System/Library/Frameworks/JavaVM.framework/Versions/CurrentJDK/Libraries/libJavaEmbedding.dylib"));
  if (javaBundle) return CFBundleGetFunctionPointerForName(javaBundle, functionName);
  return NULL;
}

static void* getSystemFunction(CFStringRef functionName)
{
  static CFBundleRef systemBundle = getBundle(CFSTR("/System/Library/Frameworks/System.framework"));
  if (systemBundle) return CFBundleGetFunctionPointerForName(systemBundle, functionName);
  return NULL;
}

typedef void (*sync_proc_ptr) (void);
static sync_proc_ptr sync_ptr = (sync_proc_ptr) getSystemFunction(CFSTR("sync"));

static void sync()
{
	if (sync_ptr) sync_ptr();
}

// Useful Carbon-CFM debugging tool, printf that goes to the system console.

typedef int (*printf_proc_ptr) (const char* format, ...);
static printf_proc_ptr kprintf = (printf_proc_ptr) getSystemFunction(CFSTR("printf"));

typedef jint JNICALL (*JNI_GetDefaultJavaVMInitArgs_proc_ptr) (void *args);
static JNI_GetDefaultJavaVMInitArgs_proc_ptr _JNI_GetDefaultJavaVMInitArgs = (JNI_GetDefaultJavaVMInitArgs_proc_ptr) getJavaVMFunction(CFSTR("JNI_GetDefaultJavaVMInitArgs"));

_JNI_IMPORT_OR_EXPORT_ jint JNICALL
JNI_GetDefaultJavaVMInitArgs(void *args)
{
    kprintf("_JNI_GetDefaultJavaVMInitArgs = 0x%08X\n", _JNI_GetDefaultJavaVMInitArgs);
    if (_JNI_GetDefaultJavaVMInitArgs) return _JNI_GetDefaultJavaVMInitArgs(args);
    return -1;
}

typedef jint JNICALL (*JNI_CreateJavaVM_proc_ptr) (JavaVM **pvm, void **penv, void *args);
static JNI_CreateJavaVM_proc_ptr _JNI_CreateJavaVM = (JNI_CreateJavaVM_proc_ptr) getJavaVMFunction(CFSTR("JNI_CreateJavaVM"));

_JNI_IMPORT_OR_EXPORT_ jint JNICALL
JNI_CreateJavaVM(JavaVM **pvm, void **penv, void *args)
{
    kprintf("_JNI_CreateJavaVM = 0x%08X\n", _JNI_CreateJavaVM);
    if (_JNI_CreateJavaVM) return _JNI_CreateJavaVM(pvm, penv, args);
    return -1;
}
