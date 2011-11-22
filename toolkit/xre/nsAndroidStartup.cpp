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
 * The Original Code is Android port code.
 *
 * The Initial Developer of the Original Code is
 *   Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Vladimir Vukicevic <vladimir@pobox.com>
 *   Michael Wu <mwu@mozilla.com>
 *   Brad Lassey <blassey@mozilla.com>
 *   Alex Pakhotin <alexp@mozilla.com>
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

#include "application.ini.h"

#include <android/log.h>

#include <jni.h>

#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "nsTArray.h"
#include "nsString.h"
#include "nsILocalFile.h"
#include "nsAppRunner.h"
#include "AndroidBridge.h"
#include "APKOpen.h"
#include "nsExceptionHandler.h"

#define LOG(args...) __android_log_print(ANDROID_LOG_INFO, MOZ_APP_NAME, args)

struct AutoAttachJavaThread {
    AutoAttachJavaThread() {
        attached = mozilla_AndroidBridge_SetMainThread((void*)pthread_self());
    }
    ~AutoAttachJavaThread() {
        mozilla_AndroidBridge_SetMainThread(nsnull);
        attached = false;
    }

    bool attached;
};

static void*
GeckoStart(void *data)
{
#ifdef MOZ_CRASHREPORTER
    const struct mapping_info *info = getLibraryMapping();
    while (info->name) {
      CrashReporter::AddLibraryMapping(info->name, info->file_id, info->base,
                                       info->len, info->offset);
      info++;
    }
#endif

    AutoAttachJavaThread attacher;
    if (!attacher.attached)
        return 0;

    if (!data) {
        LOG("Failed to get arguments for GeckoStart\n");
        return 0;
    }

    nsTArray<char *> targs;
    char *arg = strtok(static_cast<char *>(data), " ");
    while (arg) {
        targs.AppendElement(arg);
        arg = strtok(NULL, " ");
    }
    targs.AppendElement(static_cast<char *>(nsnull));

    int result = XRE_main(targs.Length() - 1, targs.Elements(), &sAppData);

    if (result)
        LOG("XRE_main returned %d", result);

    mozilla::AndroidBridge::Bridge()->NotifyXreExit();

    free(targs[0]);
    nsMemory::Free(data);
    return 0;
}

extern "C" NS_EXPORT void JNICALL
Java_org_mozilla_gecko_GeckoAppShell_nativeRun(JNIEnv *jenv, jclass jc, jstring jargs)
{
    // We need to put Gecko on a even more separate thread, because
    // otherwise this JNI method never returns; this leads to problems
    // with local references overrunning the local refs table, among
    // other things, since GC can't ever run on them.

    // Note that we don't have xpcom initialized yet, so we can't use the
    // thread manager for this.  Instead, we use pthreads directly.

    nsAutoString wargs;
    int len = jenv->GetStringLength(jargs);
    wargs.SetLength(jenv->GetStringLength(jargs));
    jenv->GetStringRegion(jargs, 0, len, wargs.BeginWriting());
    char *args = ToNewUTF8String(wargs);

    GeckoStart(args);
}

