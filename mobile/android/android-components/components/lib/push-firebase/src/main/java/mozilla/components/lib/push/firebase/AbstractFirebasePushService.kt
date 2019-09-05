/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 *  License, v. 2.0. If a copy of the MPL was not distributed with this
 *  file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

package mozilla.components.lib.push.firebase

import android.content.Context
import com.google.android.gms.common.ConnectionResult
import com.google.android.gms.common.GoogleApiAvailability
import com.google.firebase.FirebaseApp
import com.google.firebase.iid.FirebaseInstanceId
import com.google.firebase.messaging.FirebaseMessaging
import com.google.firebase.messaging.FirebaseMessagingService
import com.google.firebase.messaging.RemoteMessage
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import mozilla.components.concept.push.EncryptedPushMessage
import mozilla.components.concept.push.PushError
import mozilla.components.concept.push.PushProcessor
import mozilla.components.concept.push.PushService
import mozilla.components.support.base.log.logger.Logger
import java.io.IOException
import kotlin.coroutines.CoroutineContext

/**
 * A Firebase Cloud Messaging implementation of the [PushService] for Android devices that support Google Play Services.
 */
abstract class AbstractFirebasePushService(
    internal val coroutineContext: CoroutineContext = Dispatchers.IO
) : FirebaseMessagingService(), PushService {

    private val logger = Logger("AbstractFirebasePushService")

    /**
     * Initializes Firebase and starts the messaging service if not already started and enables auto-start as well.
     */
    override fun start(context: Context) {
        FirebaseApp.initializeApp(context)
        FirebaseMessaging.getInstance().isAutoInitEnabled = true
    }

    override fun onNewToken(newToken: String) {
        PushProcessor.requireInstance.onNewToken(newToken)
    }

    override fun onMessageReceived(remoteMessage: RemoteMessage?) {
        remoteMessage?.let {
            try {
                val message = EncryptedPushMessage(
                    channelId = it.data.getValue("chid"),
                    body = it.data.getValue("body"),
                    encoding = it.data.getValue("con"),
                    salt = it.data["enc"],
                    cryptoKey = it.data["cryptokey"]
                )
                PushProcessor.requireInstance.onMessageReceived(message)
            } catch (e: NoSuchElementException) {
                PushProcessor.requireInstance.onError(
                    PushError.MalformedMessage("parsing encrypted message failed: $e")
                )
            }
        }
    }

    /**
     * Stops the Firebase messaging service and disables auto-start.
     */
    final override fun stop() {
        FirebaseMessaging.getInstance().isAutoInitEnabled = false
        stopSelf()
    }

    /**
     * Removes the Firebase instance ID. This would lead a new token being generated when the
     * service hits the Firebase servers.
     */
    override fun deleteToken() {
        CoroutineScope(coroutineContext).launch {
            try {
                FirebaseInstanceId.getInstance().deleteInstanceId()
            } catch (e: IOException) {
                logger.error("Force registration renewable failed.", e)
            }
        }
    }

    override fun isServiceAvailable(context: Context): Boolean {
        return GoogleApiAvailability.getInstance().isGooglePlayServicesAvailable(context) == ConnectionResult.SUCCESS
    }
}
