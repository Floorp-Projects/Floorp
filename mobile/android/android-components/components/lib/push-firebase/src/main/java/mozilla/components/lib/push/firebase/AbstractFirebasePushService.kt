/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.push.firebase

import android.content.Context
import com.google.android.gms.common.ConnectionResult
import com.google.android.gms.common.GoogleApiAvailability
import com.google.firebase.FirebaseApp
import com.google.firebase.iid.FirebaseInstanceId
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
    }

    override fun onNewToken(newToken: String) {
        PushProcessor.requireInstance.onNewToken(newToken)
    }

    @SuppressWarnings("TooGenericExceptionCaught")
    override fun onMessageReceived(remoteMessage: RemoteMessage?) {
        remoteMessage?.let {
            // This is not an AutoPush message we can handle.
            if (it.data[MESSAGE_KEY_CHANNEL_ID] == null) {
                return
            }

            val message = try {
                EncryptedPushMessage(
                    channelId = it.data.getValue(MESSAGE_KEY_CHANNEL_ID),
                    body = it.data.getValue(MESSAGE_KEY_BODY),
                    encoding = it.data.getValue(MESSAGE_KEY_ENCODING),
                    salt = it.data[MESSAGE_KEY_SALT],
                    cryptoKey = it.data[MESSAGE_KEY_CRYPTO_KEY]
                )
            } catch (e: NoSuchElementException) {
                PushProcessor.requireInstance.onError(
                    PushError.MalformedMessage("parsing encrypted message failed: $e")
                )
                return
            }

            // In case of any errors, let the PushProcessor handle this exception. Instead of crashing
            // here, just drop the message on the floor. This is fine, since we don't really need to
            // "recover" from a bad incoming message.
            // PushProcessor will submit relevant issues via a CrashReporter as appropriate.
            try {
                PushProcessor.requireInstance.onMessageReceived(message)
            } catch (e: IllegalStateException) {
                // Re-throw 'requireInstance' exceptions.
                throw(e)
            } catch (e: Exception) {
                PushProcessor.requireInstance.onError(PushError.Rust(e))
            }
        }
    }

    /**
     * Stops the Firebase messaging service and disables auto-start.
     */
    final override fun stop() {
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

    companion object {
        const val MESSAGE_KEY_CHANNEL_ID = "chid"
        const val MESSAGE_KEY_BODY = "body"
        const val MESSAGE_KEY_ENCODING = "con"
        const val MESSAGE_KEY_SALT = "enc"
        const val MESSAGE_KEY_CRYPTO_KEY = "cryptokey"
    }
}
