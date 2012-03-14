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
#include "AndroidGraphicBuffer.h"

#include <jni.h>
#include <pthread.h>
#include <dlfcn.h>
#include <stdio.h>

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

#include "mozilla/unused.h"

#include "mozilla/dom/sms/SmsMessage.h"
#include "mozilla/dom/sms/Constants.h"
#include "mozilla/dom/sms/Types.h"
#include "mozilla/dom/sms/PSms.h"
#include "mozilla/dom/sms/SmsParent.h"
#include "nsISmsRequestManager.h"
#include "nsISmsDatabaseService.h"

using namespace mozilla;
using namespace mozilla::dom::sms;

/* Forward declare all the JNI methods as extern "C" */

extern "C" {
    NS_EXPORT void JNICALL Java_org_mozilla_gecko_GeckoAppShell_nativeInit(JNIEnv *, jclass);
    NS_EXPORT void JNICALL Java_org_mozilla_gecko_GeckoAppShell_notifyGeckoOfEvent(JNIEnv *, jclass, jobject event);
    NS_EXPORT void JNICALL Java_org_mozilla_gecko_GeckoAppShell_processNextNativeEvent(JNIEnv *, jclass);
    NS_EXPORT void JNICALL Java_org_mozilla_gecko_GeckoAppShell_setLayerClient(JNIEnv *jenv, jclass, jobject sv);
    NS_EXPORT void JNICALL Java_org_mozilla_gecko_GeckoAppShell_setSurfaceView(JNIEnv *jenv, jclass, jobject sv);
    NS_EXPORT void JNICALL Java_org_mozilla_gecko_GeckoAppShell_onResume(JNIEnv *, jclass);
    NS_EXPORT void JNICALL Java_org_mozilla_gecko_GeckoAppShell_onLowMemory(JNIEnv *, jclass);
    NS_EXPORT void JNICALL Java_org_mozilla_gecko_GeckoAppShell_callObserver(JNIEnv *, jclass, jstring observerKey, jstring topic, jstring data);
    NS_EXPORT void JNICALL Java_org_mozilla_gecko_GeckoAppShell_removeObserver(JNIEnv *jenv, jclass, jstring jObserverKey);
    NS_EXPORT void JNICALL Java_org_mozilla_gecko_GeckoAppShell_onChangeNetworkLinkStatus(JNIEnv *, jclass, jstring status);
    NS_EXPORT void JNICALL Java_org_mozilla_gecko_GeckoAppShell_reportJavaCrash(JNIEnv *, jclass, jstring stack);
    NS_EXPORT void JNICALL Java_org_mozilla_gecko_GeckoAppShell_executeNextRunnable(JNIEnv *, jclass);
    NS_EXPORT void JNICALL Java_org_mozilla_gecko_GeckoAppShell_notifyUriVisited(JNIEnv *, jclass, jstring uri);
    NS_EXPORT void JNICALL Java_org_mozilla_gecko_GeckoAppShell_notifyBatteryChange(JNIEnv* jenv, jclass, jdouble, jboolean, jdouble);
    NS_EXPORT void JNICALL Java_org_mozilla_gecko_GeckoAppShell_notifySmsReceived(JNIEnv* jenv, jclass, jstring, jstring, jlong);
    NS_EXPORT PRInt32 JNICALL Java_org_mozilla_gecko_GeckoAppShell_saveMessageInSentbox(JNIEnv* jenv, jclass, jstring, jstring, jlong);
    NS_EXPORT void JNICALL Java_org_mozilla_gecko_GeckoAppShell_notifySmsSent(JNIEnv* jenv, jclass, jint, jstring, jstring, jlong, jint, jlong);
    NS_EXPORT void JNICALL Java_org_mozilla_gecko_GeckoAppShell_notifySmsDelivered(JNIEnv* jenv, jclass, jint, jstring, jstring, jlong);
    NS_EXPORT void JNICALL Java_org_mozilla_gecko_GeckoAppShell_notifySmsSendFailed(JNIEnv* jenv, jclass, jint, jint, jlong);
    NS_EXPORT void JNICALL Java_org_mozilla_gecko_GeckoAppShell_notifyGetSms(JNIEnv* jenv, jclass, jint, jstring, jstring, jstring, jlong, jint, jlong);
    NS_EXPORT void JNICALL Java_org_mozilla_gecko_GeckoAppShell_notifyGetSmsFailed(JNIEnv* jenv, jclass, jint, jint, jlong);
    NS_EXPORT void JNICALL Java_org_mozilla_gecko_GeckoAppShell_notifySmsDeleted(JNIEnv* jenv, jclass, jboolean, jint, jlong);
    NS_EXPORT void JNICALL Java_org_mozilla_gecko_GeckoAppShell_notifySmsDeleteFailed(JNIEnv* jenv, jclass, jint, jint, jlong);
    NS_EXPORT void JNICALL Java_org_mozilla_gecko_GeckoAppShell_notifyNoMessageInList(JNIEnv* jenv, jclass, jint, jlong);
    NS_EXPORT void JNICALL Java_org_mozilla_gecko_GeckoAppShell_notifyListCreated(JNIEnv* jenv, jclass, jint, jint, jstring, jstring, jstring, jlong, jint, jlong);
    NS_EXPORT void JNICALL Java_org_mozilla_gecko_GeckoAppShell_notifyGotNextMessage(JNIEnv* jenv, jclass, jint, jstring, jstring, jstring, jlong, jint, jlong);
    NS_EXPORT void JNICALL Java_org_mozilla_gecko_GeckoAppShell_notifyReadingMessageListFailed(JNIEnv* jenv, jclass, jint, jint, jlong);

#ifdef MOZ_JAVA_COMPOSITOR
    NS_EXPORT void JNICALL Java_org_mozilla_gecko_GeckoAppShell_scheduleComposite(JNIEnv* jenv, jclass);
    NS_EXPORT void JNICALL Java_org_mozilla_gecko_GeckoAppShell_schedulePauseComposition(JNIEnv* jenv, jclass);
    NS_EXPORT void JNICALL Java_org_mozilla_gecko_GeckoAppShell_scheduleResumeComposition(JNIEnv* jenv, jclass);
#endif

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
Java_org_mozilla_gecko_GeckoAppShell_setLayerClient(JNIEnv *jenv, jclass, jobject obj)
{
    AndroidBridge::Bridge()->SetLayerClient(jenv->NewGlobalRef(obj));
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
Java_org_mozilla_gecko_GeckoAppShell_reportJavaCrash(JNIEnv *jenv, jclass, jstring jStackTrace)
{
#ifdef MOZ_CRASHREPORTER
    const nsJNIString stackTrace16(jStackTrace, jenv);
    const NS_ConvertUTF16toUTF8 stackTrace8(stackTrace16);
    CrashReporter::AnnotateCrashReport(NS_LITERAL_CSTRING("JavaStackTrace"), stackTrace8);
#endif // MOZ_CRASHREPORTER

    abort();
}

NS_EXPORT void JNICALL
Java_org_mozilla_gecko_GeckoAppShell_executeNextRunnable(JNIEnv *jenv, jclass)
{
    __android_log_print(ANDROID_LOG_INFO, "GeckoJNI", "%s", __PRETTY_FUNCTION__);
    if (!AndroidBridge::Bridge()) {
        __android_log_print(ANDROID_LOG_INFO, "GeckoJNI", "no bridge in %s!!!!", __PRETTY_FUNCTION__);
        return;
    }
    AndroidBridge::Bridge()->ExecuteNextRunnable(jenv);
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

NS_EXPORT void JNICALL
Java_org_mozilla_gecko_GeckoAppShell_notifySmsReceived(JNIEnv* jenv, jclass,
                                                       jstring aSender,
                                                       jstring aBody,
                                                       jlong aTimestamp)
{
    class NotifySmsReceivedRunnable : public nsRunnable {
    public:
      NotifySmsReceivedRunnable(const SmsMessageData& aMessageData)\
        : mMessageData(aMessageData)
      {}

      NS_IMETHODIMP Run() {
        nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
        if (!obs) {
          return NS_OK;
        }

        nsCOMPtr<nsIDOMMozSmsMessage> message = new SmsMessage(mMessageData);
        obs->NotifyObservers(message, kSmsReceivedObserverTopic, nsnull);
        return NS_OK;
      }

    private:
      SmsMessageData mMessageData;
    };

    SmsMessageData message(0, eDeliveryState_Received, nsJNIString(aSender, jenv), EmptyString(),
                           nsJNIString(aBody, jenv), aTimestamp);

    nsCOMPtr<nsIRunnable> runnable = new NotifySmsReceivedRunnable(message);
    NS_DispatchToMainThread(runnable);
}

NS_EXPORT PRInt32 JNICALL
Java_org_mozilla_gecko_GeckoAppShell_saveMessageInSentbox(JNIEnv* jenv, jclass,
                                                          jstring aReceiver,
                                                          jstring aBody,
                                                          jlong aTimestamp)
{
    nsCOMPtr<nsISmsDatabaseService> smsDBService =
      do_GetService(SMS_DATABASE_SERVICE_CONTRACTID);

    if (!smsDBService) {
      NS_ERROR("Sms Database Service not available!");
      return -1;
    }

    PRInt32 id;
    smsDBService->SaveSentMessage(nsJNIString(aReceiver, jenv),
                                  nsJNIString(aBody, jenv), aTimestamp, &id);

    return id;
}

NS_EXPORT void JNICALL
Java_org_mozilla_gecko_GeckoAppShell_notifySmsSent(JNIEnv* jenv, jclass,
                                                   jint aId,
                                                   jstring aReceiver,
                                                   jstring aBody,
                                                   jlong aTimestamp,
                                                   jint aRequestId,
                                                   jlong aProcessId)
{
    class NotifySmsSentRunnable : public nsRunnable {
    public:
      NotifySmsSentRunnable(const SmsMessageData& aMessageData,
                            PRInt32 aRequestId, PRUint64 aProcessId)
        : mMessageData(aMessageData)
        , mRequestId(aRequestId)
        , mProcessId(aProcessId)
      {}

      NS_IMETHODIMP Run() {
        /*
         * First, we are going to notify all SmsManager that a message has
         * been sent. Then, we will notify the SmsRequest object about it.
         */
        nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
        if (!obs) {
          return NS_OK;
        }

        nsCOMPtr<nsIDOMMozSmsMessage> message = new SmsMessage(mMessageData);
        obs->NotifyObservers(message, kSmsSentObserverTopic, nsnull);

        if (mProcessId == 0) { // Parent process.
          nsCOMPtr<nsISmsRequestManager> requestManager
            = do_GetService(SMS_REQUEST_MANAGER_CONTRACTID);
          if (requestManager) {
            requestManager->NotifySmsSent(mRequestId, message);
          }
        } else { // Content process.
          nsTArray<SmsParent*> spList;
          SmsParent::GetAll(spList);

          for (PRUint32 i=0; i<spList.Length(); ++i) {
            unused << spList[i]->SendNotifyRequestSmsSent(mMessageData,
                                                          mRequestId,
                                                          mProcessId);
          }
        }

        return NS_OK;
      }

    private:
      SmsMessageData mMessageData;
      PRInt32        mRequestId;
      PRUint64       mProcessId;
    };

    SmsMessageData message(aId, eDeliveryState_Sent, EmptyString(),
                           nsJNIString(aReceiver, jenv),
                           nsJNIString(aBody, jenv), aTimestamp);

    nsCOMPtr<nsIRunnable> runnable = new NotifySmsSentRunnable(message, aRequestId, aProcessId);
    NS_DispatchToMainThread(runnable);
}

NS_EXPORT void JNICALL
Java_org_mozilla_gecko_GeckoAppShell_notifySmsDelivered(JNIEnv* jenv, jclass,
                                                        jint aId,
                                                        jstring aReceiver,
                                                        jstring aBody,
                                                        jlong aTimestamp)
{
    class NotifySmsDeliveredRunnable : public nsRunnable {
    public:
      NotifySmsDeliveredRunnable(const SmsMessageData& aMessageData)
        : mMessageData(aMessageData)
      {}

      NS_IMETHODIMP Run() {
        nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
        if (!obs) {
          return NS_OK;
        }

        nsCOMPtr<nsIDOMMozSmsMessage> message = new SmsMessage(mMessageData);
        obs->NotifyObservers(message, kSmsDeliveredObserverTopic, nsnull);

        return NS_OK;
      }

    private:
      SmsMessageData mMessageData;
    };

    SmsMessageData message(aId, eDeliveryState_Sent, EmptyString(),
                           nsJNIString(aReceiver, jenv),
                           nsJNIString(aBody, jenv), aTimestamp);

    nsCOMPtr<nsIRunnable> runnable = new NotifySmsDeliveredRunnable(message);
    NS_DispatchToMainThread(runnable);
}

NS_EXPORT void JNICALL
Java_org_mozilla_gecko_GeckoAppShell_notifySmsSendFailed(JNIEnv* jenv, jclass,
                                                         jint aError,
                                                         jint aRequestId,
                                                         jlong aProcessId)
{
    class NotifySmsSendFailedRunnable : public nsRunnable {
    public:
      NotifySmsSendFailedRunnable(PRInt32 aError,
                                  PRInt32 aRequestId,
                                  PRUint64 aProcessId)
        : mError(aError)
        , mRequestId(aRequestId)
        , mProcessId(aProcessId)
      {}

      NS_IMETHODIMP Run() {
        if (mProcessId == 0) { // Parent process.
          nsCOMPtr<nsISmsRequestManager> requestManager
            = do_GetService(SMS_REQUEST_MANAGER_CONTRACTID);
          if (requestManager) {
            requestManager->NotifySmsSendFailed(mRequestId, mError);
          }
        } else { // Content process.
          nsTArray<SmsParent*> spList;
          SmsParent::GetAll(spList);

          for (PRUint32 i=0; i<spList.Length(); ++i) {
            unused << spList[i]->SendNotifyRequestSmsSendFailed(mError,
                                                                mRequestId,
                                                                mProcessId);
          }
        }

        return NS_OK;
      }

    private:
      PRInt32  mError;
      PRInt32  mRequestId;
      PRUint64 mProcessId;
    };


    nsCOMPtr<nsIRunnable> runnable =
      new NotifySmsSendFailedRunnable(aError, aRequestId, aProcessId);
    NS_DispatchToMainThread(runnable);
}

NS_EXPORT void JNICALL
Java_org_mozilla_gecko_GeckoAppShell_notifyGetSms(JNIEnv* jenv, jclass,
                                                  jint aId,
                                                  jstring aReceiver,
                                                  jstring aSender,
                                                  jstring aBody,
                                                  jlong aTimestamp,
                                                  jint aRequestId,
                                                  jlong aProcessId)
{
    class NotifyGetSmsRunnable : public nsRunnable {
    public:
      NotifyGetSmsRunnable(const SmsMessageData& aMessageData,
                            PRInt32 aRequestId, PRUint64 aProcessId)
        : mMessageData(aMessageData)
        , mRequestId(aRequestId)
        , mProcessId(aProcessId)
      {}

      NS_IMETHODIMP Run() {
        if (mProcessId == 0) { // Parent process.
          nsCOMPtr<nsIDOMMozSmsMessage> message = new SmsMessage(mMessageData);
          nsCOMPtr<nsISmsRequestManager> requestManager
            = do_GetService(SMS_REQUEST_MANAGER_CONTRACTID);
          if (requestManager) {
            requestManager->NotifyGotSms(mRequestId, message);
          }
        } else { // Content process.
          nsTArray<SmsParent*> spList;
          SmsParent::GetAll(spList);

          for (PRUint32 i=0; i<spList.Length(); ++i) {
            unused << spList[i]->SendNotifyRequestGotSms(mMessageData,
                                                         mRequestId,
                                                         mProcessId);
          }
        }

        return NS_OK;
      }

    private:
      SmsMessageData mMessageData;
      PRInt32        mRequestId;
      PRUint64       mProcessId;
    };

    nsJNIString receiver = nsJNIString(aReceiver, jenv);
    DeliveryState state = receiver.IsEmpty() ? eDeliveryState_Received
                                             : eDeliveryState_Sent;

    SmsMessageData message(aId, state, nsJNIString(aSender, jenv), receiver,
                           nsJNIString(aBody, jenv), aTimestamp);

    nsCOMPtr<nsIRunnable> runnable = new NotifyGetSmsRunnable(message, aRequestId, aProcessId);
    NS_DispatchToMainThread(runnable);
}

NS_EXPORT void JNICALL
Java_org_mozilla_gecko_GeckoAppShell_notifyGetSmsFailed(JNIEnv* jenv, jclass,
                                                        jint aError,
                                                        jint aRequestId,
                                                        jlong aProcessId)
{
    class NotifyGetSmsFailedRunnable : public nsRunnable {
    public:
      NotifyGetSmsFailedRunnable(PRInt32 aError,
                                 PRInt32 aRequestId,
                                 PRUint64 aProcessId)
        : mError(aError)
        , mRequestId(aRequestId)
        , mProcessId(aProcessId)
      {}

      NS_IMETHODIMP Run() {
        if (mProcessId == 0) { // Parent process.
          nsCOMPtr<nsISmsRequestManager> requestManager
            = do_GetService(SMS_REQUEST_MANAGER_CONTRACTID);
          if (requestManager) {
            requestManager->NotifyGetSmsFailed(mRequestId, mError);
          }
        } else { // Content process.
          nsTArray<SmsParent*> spList;
          SmsParent::GetAll(spList);

          for (PRUint32 i=0; i<spList.Length(); ++i) {
            unused << spList[i]->SendNotifyRequestGetSmsFailed(mError,
                                                               mRequestId,
                                                               mProcessId);
          }
        }

        return NS_OK;
      }

    private:
      PRInt32  mError;
      PRInt32  mRequestId;
      PRUint64 mProcessId;
    };


    nsCOMPtr<nsIRunnable> runnable =
      new NotifyGetSmsFailedRunnable(aError, aRequestId, aProcessId);
    NS_DispatchToMainThread(runnable);
}

NS_EXPORT void JNICALL
Java_org_mozilla_gecko_GeckoAppShell_notifySmsDeleted(JNIEnv* jenv, jclass,
                                                      jboolean aDeleted,
                                                      jint aRequestId,
                                                      jlong aProcessId)
{
    class NotifySmsDeletedRunnable : public nsRunnable {
    public:
      NotifySmsDeletedRunnable(bool aDeleted, PRInt32 aRequestId,
                               PRUint64 aProcessId)
        : mDeleted(aDeleted)
        , mRequestId(aRequestId)
        , mProcessId(aProcessId)
      {}

      NS_IMETHODIMP Run() {
        if (mProcessId == 0) { // Parent process.
          nsCOMPtr<nsISmsRequestManager> requestManager
            = do_GetService(SMS_REQUEST_MANAGER_CONTRACTID);
          if (requestManager) {
            requestManager->NotifySmsDeleted(mRequestId, mDeleted);
          }
        } else { // Content process.
          nsTArray<SmsParent*> spList;
          SmsParent::GetAll(spList);

          for (PRUint32 i=0; i<spList.Length(); ++i) {
            unused << spList[i]->SendNotifyRequestSmsDeleted(mDeleted,
                                                             mRequestId,
                                                             mProcessId);
          }
        }

        return NS_OK;
      }

    private:
      bool      mDeleted;
      PRInt32   mRequestId;
      PRUint64  mProcessId;
    };


    nsCOMPtr<nsIRunnable> runnable =
      new NotifySmsDeletedRunnable(aDeleted, aRequestId, aProcessId);
    NS_DispatchToMainThread(runnable);
}

NS_EXPORT void JNICALL
Java_org_mozilla_gecko_GeckoAppShell_notifySmsDeleteFailed(JNIEnv* jenv, jclass,
                                                           jint aError,
                                                           jint aRequestId,
                                                           jlong aProcessId)
{
    class NotifySmsDeleteFailedRunnable : public nsRunnable {
    public:
      NotifySmsDeleteFailedRunnable(PRInt32 aError,
                                    PRInt32 aRequestId,
                                    PRUint64 aProcessId)
        : mError(aError)
        , mRequestId(aRequestId)
        , mProcessId(aProcessId)
      {}

      NS_IMETHODIMP Run() {
        if (mProcessId == 0) { // Parent process.
          nsCOMPtr<nsISmsRequestManager> requestManager
            = do_GetService(SMS_REQUEST_MANAGER_CONTRACTID);
          if (requestManager) {
            requestManager->NotifySmsDeleteFailed(mRequestId, mError);
          }
        } else { // Content process.
          nsTArray<SmsParent*> spList;
          SmsParent::GetAll(spList);

          for (PRUint32 i=0; i<spList.Length(); ++i) {
            unused << spList[i]->SendNotifyRequestSmsDeleteFailed(mError,
                                                                  mRequestId,
                                                                  mProcessId);
          }
        }

        return NS_OK;
      }

    private:
      PRInt32  mError;
      PRInt32  mRequestId;
      PRUint64 mProcessId;
    };


    nsCOMPtr<nsIRunnable> runnable =
      new NotifySmsDeleteFailedRunnable(aError, aRequestId, aProcessId);
    NS_DispatchToMainThread(runnable);
}

NS_EXPORT void JNICALL
Java_org_mozilla_gecko_GeckoAppShell_notifyNoMessageInList(JNIEnv* jenv, jclass,
                                                           jint aRequestId,
                                                           jlong aProcessId)
{
    class NotifyNoMessageInListRunnable : public nsRunnable {
    public:
      NotifyNoMessageInListRunnable(PRInt32 aRequestId, PRUint64 aProcessId)
        : mRequestId(aRequestId)
        , mProcessId(aProcessId)
      {}

      NS_IMETHODIMP Run() {
        if (mProcessId == 0) { // Parent process.
          nsCOMPtr<nsISmsRequestManager> requestManager
            = do_GetService(SMS_REQUEST_MANAGER_CONTRACTID);
          if (requestManager) {
            requestManager->NotifyNoMessageInList(mRequestId);
          }
        } else { // Content process.
          nsTArray<SmsParent*> spList;
          SmsParent::GetAll(spList);

          for (PRUint32 i=0; i<spList.Length(); ++i) {
            unused << spList[i]->SendNotifyRequestNoMessageInList(mRequestId,
                                                                  mProcessId);
          }
        }

        return NS_OK;
      }

    private:
      PRInt32               mRequestId;
      PRUint64              mProcessId;
    };


    nsCOMPtr<nsIRunnable> runnable =
      new NotifyNoMessageInListRunnable(aRequestId, aProcessId);
    NS_DispatchToMainThread(runnable);
}

NS_EXPORT void JNICALL
Java_org_mozilla_gecko_GeckoAppShell_notifyListCreated(JNIEnv* jenv, jclass,
                                                       jint aListId,
                                                       jint aMessageId,
                                                       jstring aReceiver,
                                                       jstring aSender,
                                                       jstring aBody,
                                                       jlong aTimestamp,
                                                       jint aRequestId,
                                                       jlong aProcessId)
{
    class NotifyCreateMessageListRunnable : public nsRunnable {
    public:
      NotifyCreateMessageListRunnable(PRInt32 aListId,
                                      const SmsMessageData& aMessage,
                                      PRInt32 aRequestId, PRUint64 aProcessId)
        : mListId(aListId)
        , mMessage(aMessage)
        , mRequestId(aRequestId)
        , mProcessId(aProcessId)
      {}

      NS_IMETHODIMP Run() {
        if (mProcessId == 0) { // Parent process.
          nsCOMPtr<nsIDOMMozSmsMessage> message = new SmsMessage(mMessage);
          nsCOMPtr<nsISmsRequestManager> requestManager
            = do_GetService(SMS_REQUEST_MANAGER_CONTRACTID);
          if (requestManager) {
            requestManager->NotifyCreateMessageList(mRequestId,
                                                    mListId,
                                                    message);
          }
        } else { // Content process.
          nsTArray<SmsParent*> spList;
          SmsParent::GetAll(spList);

          for (PRUint32 i=0; i<spList.Length(); ++i) {
            unused << spList[i]->SendNotifyRequestCreateMessageList(mListId,
                                                                    mMessage,
                                                                    mRequestId,
                                                                    mProcessId);
          }
        }

        return NS_OK;
      }

    private:
      PRInt32        mListId;
      SmsMessageData mMessage;
      PRInt32        mRequestId;
      PRUint64       mProcessId;
    };


    nsJNIString receiver = nsJNIString(aReceiver, jenv);
    DeliveryState state = receiver.IsEmpty() ? eDeliveryState_Received
                                             : eDeliveryState_Sent;

    SmsMessageData message(aMessageId, state, nsJNIString(aSender, jenv),
                           receiver, nsJNIString(aBody, jenv), aTimestamp);

    nsCOMPtr<nsIRunnable> runnable =
      new NotifyCreateMessageListRunnable(aListId, message, aRequestId, aProcessId);
    NS_DispatchToMainThread(runnable);
}

NS_EXPORT void JNICALL
Java_org_mozilla_gecko_GeckoAppShell_notifyGotNextMessage(JNIEnv* jenv, jclass,
                                                          jint aMessageId,
                                                          jstring aReceiver,
                                                          jstring aSender,
                                                          jstring aBody,
                                                          jlong aTimestamp,
                                                          jint aRequestId,
                                                          jlong aProcessId)
{
    class NotifyGotNextMessageRunnable : public nsRunnable {
    public:
      NotifyGotNextMessageRunnable(const SmsMessageData& aMessage,
                                   PRInt32 aRequestId, PRUint64 aProcessId)
        : mMessage(aMessage)
        , mRequestId(aRequestId)
        , mProcessId(aProcessId)
      {}

      NS_IMETHODIMP Run() {
        if (mProcessId == 0) { // Parent process.
          nsCOMPtr<nsIDOMMozSmsMessage> message = new SmsMessage(mMessage);
          nsCOMPtr<nsISmsRequestManager> requestManager
            = do_GetService(SMS_REQUEST_MANAGER_CONTRACTID);
          if (requestManager) {
            requestManager->NotifyGotNextMessage(mRequestId, message);
          }
        } else { // Content process.
          nsTArray<SmsParent*> spList;
          SmsParent::GetAll(spList);

          for (PRUint32 i=0; i<spList.Length(); ++i) {
            unused << spList[i]->SendNotifyRequestGotNextMessage(mMessage,
                                                                 mRequestId,
                                                                 mProcessId);
          }
        }

        return NS_OK;
      }

    private:
      SmsMessageData mMessage;
      PRInt32        mRequestId;
      PRUint64       mProcessId;
    };


    nsJNIString receiver = nsJNIString(aReceiver, jenv);
    DeliveryState state = receiver.IsEmpty() ? eDeliveryState_Received
                                             : eDeliveryState_Sent;

    SmsMessageData message(aMessageId, state, nsJNIString(aSender, jenv),
                           receiver, nsJNIString(aBody, jenv), aTimestamp);

    nsCOMPtr<nsIRunnable> runnable =
      new NotifyGotNextMessageRunnable(message, aRequestId, aProcessId);
    NS_DispatchToMainThread(runnable);
}

NS_EXPORT void JNICALL
Java_org_mozilla_gecko_GeckoAppShell_notifyReadingMessageListFailed(JNIEnv* jenv, jclass,
                                                                    jint aError,
                                                                    jint aRequestId,
                                                                    jlong aProcessId)
{
    class NotifyReadListFailedRunnable : public nsRunnable {
    public:
      NotifyReadListFailedRunnable(PRInt32 aError,
                                   PRInt32 aRequestId,
                                   PRUint64 aProcessId)
        : mError(aError)
        , mRequestId(aRequestId)
        , mProcessId(aProcessId)
      {}

      NS_IMETHODIMP Run() {
        if (mProcessId == 0) { // Parent process.
          nsCOMPtr<nsISmsRequestManager> requestManager
            = do_GetService(SMS_REQUEST_MANAGER_CONTRACTID);
          if (requestManager) {
            requestManager->NotifyReadMessageListFailed(mRequestId, mError);
          }
        } else { // Content process.
          nsTArray<SmsParent*> spList;
          SmsParent::GetAll(spList);

          for (PRUint32 i=0; i<spList.Length(); ++i) {
            unused << spList[i]->SendNotifyRequestReadListFailed(mError,
                                                                 mRequestId,
                                                                 mProcessId);
          }
        }

        return NS_OK;
      }

    private:
      PRInt32  mError;
      PRInt32  mRequestId;
      PRUint64 mProcessId;
    };


    nsCOMPtr<nsIRunnable> runnable =
      new NotifyReadListFailedRunnable(aError, aRequestId, aProcessId);
    NS_DispatchToMainThread(runnable);
}

#ifdef MOZ_JAVA_COMPOSITOR

NS_EXPORT void JNICALL
Java_org_mozilla_gecko_GeckoAppShell_scheduleComposite(JNIEnv*, jclass)
{
    nsWindow::ScheduleComposite();
}

NS_EXPORT void JNICALL
Java_org_mozilla_gecko_GeckoAppShell_schedulePauseComposition(JNIEnv*, jclass)
{
    nsWindow::SchedulePauseComposition();
}

NS_EXPORT void JNICALL
Java_org_mozilla_gecko_GeckoAppShell_scheduleResumeComposition(JNIEnv*, jclass)
{
    nsWindow::ScheduleResumeComposition();
}

#endif
