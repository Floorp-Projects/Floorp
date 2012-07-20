/* -*- Mode: c++; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Hal.h"
#include "nsIFile.h"
#include "nsString.h"

#include "AndroidBridge.h"
#include "AndroidGraphicBuffer.h"

#include <jni.h>
#include <pthread.h>
#include <dlfcn.h>
#include <stdio.h>
#include <unistd.h>

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
#include "nsPluginInstanceOwner.h"
#include "nsSurfaceTexture.h"

using namespace mozilla;
using namespace mozilla::dom::sms;

/* Forward declare all the JNI methods as extern "C" */

extern "C" {
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
    AndroidBridge::Bridge()->SetLayerClient(jenv, obj);
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
                           nsJNIString(aBody, jenv), aTimestamp, false);

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
                           nsJNIString(aBody, jenv), aTimestamp, true);

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
                           nsJNIString(aBody, jenv), aTimestamp, true);

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

    // TODO Need to add the message `read` parameter value. Bug 748391
    SmsMessageData message(aId, state, nsJNIString(aSender, jenv), receiver,
                           nsJNIString(aBody, jenv), aTimestamp, true);

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

    // TODO Need to add the message `read` parameter value. Bug 748391
    SmsMessageData message(aMessageId, state, nsJNIString(aSender, jenv),
                           receiver, nsJNIString(aBody, jenv), aTimestamp, true);

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
 
    // TODO Need to add the message `read` parameter value. Bug 748391
    SmsMessageData message(aMessageId, state, nsJNIString(aSender, jenv),
                           receiver, nsJNIString(aBody, jenv), aTimestamp, true);

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
Java_org_mozilla_gecko_GeckoAppShell_scheduleResumeComposition(JNIEnv*, jclass, jint width, jint height)
{
    nsWindow::ScheduleResumeComposition(width, height);
}

NS_EXPORT void JNICALL
Java_org_mozilla_gecko_GeckoAppShell_notifyFilePickerResult(JNIEnv* jenv, jclass, jstring filePath, jlong callback)
{
    class NotifyFilePickerResultRunnable : public nsRunnable {
    public:
        NotifyFilePickerResultRunnable(nsString& fileDir, long callback) : 
            mFileDir(fileDir), mCallback(callback) {}

        NS_IMETHODIMP Run() {
            nsFilePickerCallback* handler = (nsFilePickerCallback*)mCallback;
            handler->handleResult(mFileDir);
            handler->Release();
            return NS_OK;
        }
    private:
        nsString mFileDir;
        long mCallback;
    };
    nsString path = nsJNIString(filePath, jenv);
    
    nsCOMPtr<nsIRunnable> runnable =
        new NotifyFilePickerResultRunnable(path, (long)callback);
    NS_DispatchToMainThread(runnable);
}

static int
NextPowerOfTwo(int value) {
    // code taken from http://acius2.blogspot.com/2007/11/calculating-next-power-of-2.html
    if (0 == value--) {
        return 1;
    }
    value = (value >> 1) | value;
    value = (value >> 2) | value;
    value = (value >> 4) | value;
    value = (value >> 8) | value;
    value = (value >> 16) | value;
    return value + 1;
}

#define MAX_LOCK_ATTEMPTS 10

static bool LockWindowWithRetry(void* window, unsigned char** bits, int* width, int* height, int* format, int* stride)
{
  int count = 0;

  while (count < MAX_LOCK_ATTEMPTS) {
      if (AndroidBridge::Bridge()->LockWindow(window, bits, width, height, format, stride))
        return true;

      count++;
      usleep(500);
  }

  return false;
}

NS_EXPORT jobject JNICALL
Java_org_mozilla_gecko_GeckoAppShell_getSurfaceBits(JNIEnv* jenv, jclass, jobject surface)
{
    static jclass jSurfaceBitsClass = nsnull;
    static jmethodID jSurfaceBitsCtor = 0;
    static jfieldID jSurfaceBitsWidth, jSurfaceBitsHeight, jSurfaceBitsFormat, jSurfaceBitsBuffer;

    jobject surfaceBits = nsnull;
    unsigned char* bitsCopy = nsnull;
    int dstWidth, dstHeight, dstSize;

    void* window = AndroidBridge::Bridge()->AcquireNativeWindow(jenv, surface);
    if (!window)
        return nsnull;

    unsigned char* bits;
    int srcWidth, srcHeight, format, srcStride;

    // So we lock/unlock once here in order to get whatever is currently the front buffer. It sucks.
    if (!LockWindowWithRetry(window, &bits, &srcWidth, &srcHeight, &format, &srcStride))
        return nsnull;

    AndroidBridge::Bridge()->UnlockWindow(window);

    // This is lock will result in the front buffer, since the last unlock rotated it to the back. Probably.
    if (!LockWindowWithRetry(window, &bits, &srcWidth, &srcHeight, &format, &srcStride))
        return nsnull;

    // These are from android.graphics.PixelFormat
    int bpp;
    switch (format) {
    case 1: // RGBA_8888
        bpp = 4;
        break;
    case 4: // RGB_565
        bpp = 2;
        break;
    default:
        goto cleanup;
    }

    dstWidth = NextPowerOfTwo(srcWidth);
    dstHeight = NextPowerOfTwo(srcHeight);
    dstSize = dstWidth * dstHeight * bpp;

    bitsCopy = (unsigned char*)malloc(dstSize);
    bzero(bitsCopy, dstSize);
    for (int i = 0; i < srcHeight; i++) {
        memcpy(bitsCopy + ((dstHeight - i - 1) * dstWidth * bpp), bits + (i * srcStride * bpp), srcStride * bpp);
    }
    
    if (!jSurfaceBitsClass) {
        jSurfaceBitsClass = (jclass)jenv->NewGlobalRef(jenv->FindClass("org/mozilla/gecko/SurfaceBits"));
        jSurfaceBitsCtor = jenv->GetMethodID(jSurfaceBitsClass, "<init>", "()V");

        jSurfaceBitsWidth = jenv->GetFieldID(jSurfaceBitsClass, "width", "I");
        jSurfaceBitsHeight = jenv->GetFieldID(jSurfaceBitsClass, "height", "I");
        jSurfaceBitsFormat = jenv->GetFieldID(jSurfaceBitsClass, "format", "I");
        jSurfaceBitsBuffer = jenv->GetFieldID(jSurfaceBitsClass, "buffer", "Ljava/nio/ByteBuffer;");
    }

    surfaceBits = jenv->NewObject(jSurfaceBitsClass, jSurfaceBitsCtor);
    jenv->SetIntField(surfaceBits, jSurfaceBitsWidth, dstWidth);
    jenv->SetIntField(surfaceBits, jSurfaceBitsHeight, dstHeight);
    jenv->SetIntField(surfaceBits, jSurfaceBitsFormat, format);
    jenv->SetObjectField(surfaceBits, jSurfaceBitsBuffer, jenv->NewDirectByteBuffer(bitsCopy, dstSize));

cleanup:
    AndroidBridge::Bridge()->UnlockWindow(window);
    AndroidBridge::Bridge()->ReleaseNativeWindow(window);

    return surfaceBits;
}

NS_EXPORT void JNICALL
Java_org_mozilla_gecko_GeckoAppShell_onFullScreenPluginHidden(JNIEnv* jenv, jclass, jobject view)
{
  class ExitFullScreenRunnable : public nsRunnable {
    public:
      ExitFullScreenRunnable(jobject view) : mView(view) {}

      NS_IMETHODIMP Run() {
        JNIEnv* env = AndroidBridge::GetJNIEnv();
        if (!env) {
          NS_WARNING("Failed to acquire JNI env, can't exit plugin fullscreen mode");
          return NS_OK;
        }

        nsPluginInstanceOwner::ExitFullScreen(mView);
        env->DeleteGlobalRef(mView);
        return NS_OK;
      }

    private:
      jobject mView;
  };

  nsCOMPtr<nsIRunnable> runnable = new ExitFullScreenRunnable(jenv->NewGlobalRef(view));
  NS_DispatchToMainThread(runnable);
}

NS_EXPORT jobject JNICALL
Java_org_mozilla_gecko_GeckoAppShell_getNextMessageFromQueue(JNIEnv* jenv, jclass, jobject queue)
{
    static jclass jMessageQueueCls = nsnull;
    static jfieldID jMessagesField;
    static jmethodID jNextMethod;
    if (!jMessageQueueCls) {
        jMessageQueueCls = (jclass) jenv->NewGlobalRef(jenv->FindClass("android/os/MessageQueue"));
        jMessagesField = jenv->GetFieldID(jMessageQueueCls, "mMessages", "Landroid/os/Message;");
        jNextMethod = jenv->GetMethodID(jMessageQueueCls, "next", "()Landroid/os/Message;");
    }
    if (!jMessageQueueCls || !jMessagesField || !jNextMethod)
        return NULL;
    jobject msg = jenv->GetObjectField(queue, jMessagesField);
    // if queue.mMessages is null, queue.next() will block, which we don't want
    if (!msg)
        return msg;
    msg = jenv->CallObjectMethod(queue, jNextMethod);
    return msg;
}

NS_EXPORT void JNICALL
Java_org_mozilla_gecko_GeckoAppShell_onSurfaceTextureFrameAvailable(JNIEnv* jenv, jclass, jobject surfaceTexture, jint id)
{
  nsSurfaceTexture* st = nsSurfaceTexture::Find(id);
  if (!st) {
    __android_log_print(ANDROID_LOG_ERROR, "GeckoJNI", "Failed to find nsSurfaceTexture with id %d", id);
    return;
  }

  st->NotifyFrameAvailable();
}

#endif
}
