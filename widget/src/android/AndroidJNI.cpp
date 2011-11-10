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
 *   Vladimir Vukicevic <vladimir@pobox.com>
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

#include "mozilla/Hal.h"
#include "nsILocalFile.h"
#include "nsString.h"

#include "AndroidBridge.h"

#include <jni.h>
#include <pthread.h>
#include <dlfcn.h>

#include "nsAppShell.h"
#include "nsWindow.h"
#include <android/log.h>
#include "nsIObserverService.h"
#include "mozilla/Services.h"
#include "nsINetworkLinkService.h"

#ifdef MOZ_CRASHREPORTER
#include "nsICrashReporter.h"
#include "nsExceptionHandler.h"
#endif


using namespace mozilla;

/* Forward declare all the JNI methods as extern "C" */

extern "C" {
    NS_EXPORT void JNICALL Java_org_mozilla_gecko_GeckoAppShell_nativeInit(JNIEnv *, jclass);
    NS_EXPORT void JNICALL Java_org_mozilla_gecko_GeckoAppShell_notifyGeckoOfEvent(JNIEnv *, jclass, jobject event);
    NS_EXPORT void JNICALL Java_org_mozilla_gecko_GeckoAppShell_processNextNativeEvent(JNIEnv *, jclass);
    NS_EXPORT void JNICALL Java_org_mozilla_gecko_GeckoAppShell_setSurfaceView(JNIEnv *jenv, jclass, jobject sv);
    NS_EXPORT void JNICALL Java_org_mozilla_gecko_GeckoAppShell_onResume(JNIEnv *, jclass);
    NS_EXPORT void JNICALL Java_org_mozilla_gecko_GeckoAppShell_onLowMemory(JNIEnv *, jclass);
    NS_EXPORT void JNICALL Java_org_mozilla_gecko_GeckoAppShell_callObserver(JNIEnv *, jclass, jstring observerKey, jstring topic, jstring data);
    NS_EXPORT void JNICALL Java_org_mozilla_gecko_GeckoAppShell_removeObserver(JNIEnv *jenv, jclass, jstring jObserverKey);
    NS_EXPORT void JNICALL Java_org_mozilla_gecko_GeckoAppShell_onChangeNetworkLinkStatus(JNIEnv *, jclass, jstring status);
    NS_EXPORT void JNICALL Java_org_mozilla_gecko_GeckoAppShell_reportJavaCrash(JNIEnv *, jclass, jstring stack);
    NS_EXPORT void JNICALL Java_org_mozilla_gecko_GeckoAppShell_executeNextRunnable(JNIEnv *, jclass);
    NS_EXPORT void JNICALL Java_org_mozilla_gecko_GeckoAppShell_notifyBatteryChange(JNIEnv* jenv, jclass, jdouble, jboolean, jdouble);
}


/*
 * Incoming JNI methods
 */

NS_EXPORT void JNICALL
Java_org_mozilla_gecko_GeckoAppShell_nativeInit(JNIEnv *jenv, jclass jc)
{
    AndroidBridge::ConstructBridge(jenv, jc);
}

NS_EXPORT void JNICALL
Java_org_mozilla_gecko_GeckoAppShell_notifyGeckoOfEvent(JNIEnv *jenv, jclass jc, jobject event)
{
    // poke the appshell
    if (nsAppShell::gAppShell)
        nsAppShell::gAppShell->PostEvent(new AndroidGeckoEvent(jenv, event));
}

NS_EXPORT void JNICALL
Java_org_mozilla_gecko_GeckoAppShell_processNextNativeEvent(JNIEnv *jenv, jclass)
{
    // poke the appshell
    if (nsAppShell::gAppShell)
        nsAppShell::gAppShell->ProcessNextNativeEvent(false);
}

NS_EXPORT void JNICALL
Java_org_mozilla_gecko_GeckoAppShell_setSurfaceView(JNIEnv *jenv, jclass, jobject obj)
{
    AndroidBridge::Bridge()->SetSurfaceView(jenv->NewGlobalRef(obj));
}

NS_EXPORT void JNICALL
Java_org_mozilla_gecko_GeckoAppShell_onLowMemory(JNIEnv *jenv, jclass jc)
{
    if (nsAppShell::gAppShell) {
        nsAppShell::gAppShell->NotifyObservers(nsnull,
                                               "memory-pressure",
                                               NS_LITERAL_STRING("low-memory").get());
    }
}

NS_EXPORT void JNICALL
Java_org_mozilla_gecko_GeckoAppShell_onResume(JNIEnv *jenv, jclass jc)
{
    if (nsAppShell::gAppShell)
        nsAppShell::gAppShell->OnResume();
}

NS_EXPORT void JNICALL
Java_org_mozilla_gecko_GeckoAppShell_callObserver(JNIEnv *jenv, jclass, jstring jObserverKey, jstring jTopic, jstring jData)
{
    if (!nsAppShell::gAppShell)
        return;

    nsJNIString sObserverKey(jObserverKey, jenv);
    nsJNIString sTopic(jTopic, jenv);
    nsJNIString sData(jData, jenv);

    nsAppShell::gAppShell->CallObserver(sObserverKey, sTopic, sData);
}

NS_EXPORT void JNICALL
Java_org_mozilla_gecko_GeckoAppShell_removeObserver(JNIEnv *jenv, jclass, jstring jObserverKey)
{
    if (!nsAppShell::gAppShell)
        return;

    const jchar *observerKey = jenv->GetStringChars(jObserverKey, NULL);
    nsString sObserverKey(observerKey);
    sObserverKey.SetLength(jenv->GetStringLength(jObserverKey));
    jenv->ReleaseStringChars(jObserverKey, observerKey);

    nsAppShell::gAppShell->RemoveObserver(sObserverKey);
}

NS_EXPORT void JNICALL
Java_org_mozilla_gecko_GeckoAppShell_onChangeNetworkLinkStatus(JNIEnv *jenv, jclass, jstring jStatus)
{
    if (!nsAppShell::gAppShell)
        return;

    nsJNIString sStatus(jStatus, jenv);

    nsAppShell::gAppShell->NotifyObservers(nsnull,
                                           NS_NETWORK_LINK_TOPIC,
                                           sStatus.get());
}

NS_EXPORT void JNICALL
Java_org_mozilla_gecko_GeckoAppShell_reportJavaCrash(JNIEnv *jenv, jclass, jstring stack)
{
#ifdef MOZ_CRASHREPORTER
    nsJNIString javaStack(stack, jenv);
    CrashReporter::AppendAppNotesToCrashReport(NS_ConvertUTF16toUTF8(javaStack));
#endif
    abort();
}

NS_EXPORT void JNICALL
Java_org_mozilla_gecko_GeckoAppShell_executeNextRunnable(JNIEnv *, jclass)
{
    __android_log_print(ANDROID_LOG_INFO, "GeckoJNI", "%s", __PRETTY_FUNCTION__);
    if (!AndroidBridge::Bridge()) {
        __android_log_print(ANDROID_LOG_INFO, "GeckoJNI", "no bridge in %s!!!!", __PRETTY_FUNCTION__);
        return;
    }
    AndroidBridge::Bridge()->ExecuteNextRunnable();
    __android_log_print(ANDROID_LOG_INFO, "GeckoJNI", "leaving %s", __PRETTY_FUNCTION__);
}

NS_EXPORT void JNICALL
Java_org_mozilla_gecko_GeckoAppShell_notifyBatteryChange(JNIEnv* jenv, jclass,
                                                         jdouble aLevel,
                                                         jboolean aCharging,
                                                         jdouble aRemainingTime)
{
    class NotifyBatteryChangeRunnable : public nsRunnable {
    public:
      NotifyBatteryChangeRunnable(double aLevel, bool aCharging, double aRemainingTime)
        : mLevel(aLevel)
        , mCharging(aCharging)
        , mRemainingTime(aRemainingTime)
      {}

      NS_IMETHODIMP Run() {
        hal::NotifyBatteryChange(hal::BatteryInformation(mLevel, mCharging, mRemainingTime));
        return NS_OK;
      }

    private:
      double mLevel;
      bool   mCharging;
      double mRemainingTime;
    };

    nsCOMPtr<nsIRunnable> runnable = new NotifyBatteryChangeRunnable(aLevel, aCharging, aRemainingTime);
    NS_DispatchToMainThread(runnable);
}

