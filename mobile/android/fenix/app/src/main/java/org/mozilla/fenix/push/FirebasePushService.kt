/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.push

import android.annotation.SuppressLint
import com.google.firebase.messaging.RemoteMessage
import com.google.firebase.messaging.FirebaseMessagingService
import com.leanplum.LeanplumPushFirebaseMessagingService
import com.leanplum.LeanplumPushService
import mozilla.components.concept.push.PushService
import mozilla.components.lib.push.firebase.AbstractFirebasePushService
import mozilla.components.feature.push.AutoPushFeature

/**
 * A wrapper class that only exists to delegate to [FirebaseMessagingService] instances.
 *
 * Implementation notes:
 *
 * This was a doozy...
 *
 * With Firebase Cloud Messaging, we've been given some tight constraints in order to get this to
 * work:
 *  - We want to have multiple FCM message receivers for AutoPush and LeanPlum (for now), however
 *  there can only be one registered [FirebaseMessagingService] in the AndroidManifest.
 *  - The [LeanplumPushFirebaseMessagingService] does not function as expected unless it's the
 *  inherited service that receives the messages.
 *  - The [AutoPushService] is not strongly tied to being the inherited service, but the
 *  [AutoPushFeature] requires a reference to the push instance as a [PushService].
 *
 * We tried creating an empty [FirebaseMessagingService] that can hold a list of the services
 * for delegating, but the [LeanplumPushFirebaseMessagingService] tries to get a reference to the
 * Application Context, however,since the FCM service runs in a background process that gives a
 * nullptr. Within LeanPlum, this is something that is probably provided internally.
 *
 * We tried to pass in an instance of the [AbstractFirebasePushService] to [FirebasePushService]
 * through the constructor and delegate the implementation of a [PushService] to that, but alas,
 * the service requires you to have an empty default constructor in order for the OS to do the
 * initialization. For this reason, we created a singleton instance of the AutoPush instance since
 * that lets us easily delegate the implementation to that, as well as make invocations when FCM
 * receives new messages.
 */
class FirebasePushService : LeanplumPushFirebaseMessagingService(),
    PushService by AutoPushService {

    override fun onCreate() {
        LeanplumPushService.setCustomizer(LeanplumNotificationCustomizer())
        super.onCreate()
    }

    override fun onNewToken(newToken: String) {
        AutoPushService.onNewToken(newToken)
        super.onNewToken(newToken)
    }

    override fun onMessageReceived(remoteMessage: RemoteMessage) {
        AutoPushService.onMessageReceived(remoteMessage)
        super.onMessageReceived(remoteMessage)
    }
}

/**
 * A singleton instance of the FirebasePushService needed for communicating between FCM and the
 * [AutoPushFeature].
 */
@SuppressLint("MissingFirebaseInstanceTokenRefresh") // Implemented internally.
object AutoPushService : AbstractFirebasePushService()
