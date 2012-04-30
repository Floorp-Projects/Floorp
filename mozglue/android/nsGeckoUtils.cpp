/* -*- Mode: c++; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
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
 * The Original Code is Mozilla Android code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Brad Lassey <blassey@mozilla.com>
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

#include <jni.h>

#ifdef MOZ_MEMORY
// Wrap malloc and free to use jemalloc
#define malloc __wrap_malloc
#define free __wrap_free
#endif

#include <stdlib.h>
#include <fcntl.h>

extern "C"
__attribute__ ((visibility("default")))
void JNICALL
Java_org_mozilla_gecko_GeckoAppShell_putenv(JNIEnv *jenv, jclass, jstring map)
{
    const char* str;
    // XXX: java doesn't give us true UTF8, we should figure out something 
    // better to do here
    str = jenv->GetStringUTFChars(map, NULL);
    if (str == NULL)
        return;
    putenv(strdup(str));
    jenv->ReleaseStringUTFChars(map, str);
}

extern "C"
__attribute__ ((visibility("default")))
jobject JNICALL
Java_org_mozilla_gecko_GeckoAppShell_allocateDirectBuffer(JNIEnv *jenv, jclass, jlong size)
{
    return jenv->NewDirectByteBuffer(malloc(size), size);
}

extern "C"
__attribute__ ((visibility("default")))
void JNICALL
Java_org_mozilla_gecko_GeckoAppShell_freeDirectBuffer(JNIEnv *jenv, jclass, jobject buf)
{
    free(jenv->GetDirectBufferAddress(buf));
}

extern "C"
__attribute__ ((visibility("default")))
void JNICALL
Java_org_mozilla_gecko_GeckoAppShell_unlockDatabaseFile(JNIEnv *jenv, jclass, jstring jDatabasePath)
{
    const char *databasePath = jenv->GetStringUTFChars(jDatabasePath, NULL);
    int fd = open(databasePath, O_RDWR);
    jenv->ReleaseStringUTFChars(jDatabasePath, databasePath);

    // File could not be open, do nothing
    if (fd < 0) {
        return;
    }

    struct flock lock;

    lock.l_type = F_WRLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0;

    int result = fcntl(fd, F_GETLK, &lock);

    // Release any existing lock in the file
    if (result != -1 && lock.l_type == F_WRLCK) {
        lock.l_type = F_UNLCK;
        fcntl(fd, F_SETLK, &lock);
    }

    close(fd);
}