/* 
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Netscape Security Services for Java.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1998-2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */

 /* 
  * registerNatives.c - 
  * 	registers statically linked native methods with the VM.
  */

#include <jni.h>
#include <assert.h>
#include <stdlib.h>

#include "registerNatives.h"
#include "nativeMethods.h"

int
registerNatives(JNIEnv *env) 
{
	jclass c;
	jint res;
	jthrowable exc;
	int j;

    if( (*env)->ExceptionOccurred(env) != NULL ) {
        fprintf(stderr,
            "ERROR: exception occurred before registering natives\n");
        exit(-1);
    }

	for (j = 0; nativeMethods[j].classname != 0; j++) {
		c = (*env)->FindClass(env, nativeMethods[j].classname); 
		if (c == 0) {
            (*env)->ExceptionDescribe(env);
			(*env)->ExceptionClear(env);
			fprintf(stderr, "Can't find %s class\n", 
					nativeMethods[j].classname);
			continue;
		}
		res = (*env)->RegisterNatives(env, c, 
									  nativeMethods[j].nat_methods,
									  nativeMethods[j].nmethods);
		exc = (*env)->ExceptionOccurred(env);
		if (exc) {
			(*env)->ExceptionDescribe(env);
			return -1;
		}
		if (res < 0) {
			fprintf(stderr, "Error in register statically linked native methods"
							"for %s\n", nativeMethods[j].classname);
			return -1;
		}
	}

	return 0;
}

