/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.push.amazon

import android.content.Context
import android.content.Intent
import android.os.Bundle
import androidx.annotation.VisibleForTesting
import com.amazon.device.messaging.ADM
import com.amazon.device.messaging.ADMMessageHandlerBase
import mozilla.components.concept.push.EncryptedPushMessage
import mozilla.components.concept.push.PushError
import mozilla.components.concept.push.PushProcessor
import mozilla.components.concept.push.PushService
import mozilla.components.concept.push.PushService.Companion.MESSAGE_KEY_BODY
import mozilla.components.concept.push.PushService.Companion.MESSAGE_KEY_CHANNEL_ID
import mozilla.components.concept.push.PushService.Companion.MESSAGE_KEY_CRYPTO_KEY
import mozilla.components.concept.push.PushService.Companion.MESSAGE_KEY_ENCODING
import mozilla.components.concept.push.PushService.Companion.MESSAGE_KEY_SALT
import mozilla.components.support.base.log.logger.Logger

/**
 * An Amazon Cloud Messaging implementation of the [PushService] for Android devices that support Google Play Services.
 * [ADMMessageHandlerBase] requires a redundant constructor parameter.
 */
abstract class AbstractAmazonPushService : ADMMessageHandlerBase("AbstractAmazonPushService"), PushService {
    private lateinit var adm: ADM
    private val logger = Logger("AbstractAmazonPushService")

    override fun start(context: Context) {
        adm = ADM(context)

        val registrationId = adm.registrationId
        if (registrationId == null) {
            adm.startRegister()
        } else {
            PushProcessor.requireInstance.onNewToken(registrationId)
        }
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PROTECTED)
    public override fun onRegistered(registrationId: String) {
        PushProcessor.requireInstance.onNewToken(registrationId)
    }

    override fun onRegistrationError(errorId: String) {
        PushProcessor.requireInstance.onError(PushError.Registration("registration failed: $errorId"))
    }

    override fun onUnregistered(registrationId: String) {
        logger.info("account no longer registered with id: $registrationId")
    }

    override fun deleteToken() {
        adm.startUnregister()
    }

    @SuppressWarnings("TooGenericExceptionCaught")
    @VisibleForTesting(otherwise = VisibleForTesting.PROTECTED)
    public override fun onMessage(intent: Intent) {
        intent.extras?.let {
            // This is not an AutoPush message we can handle.
            if (!it.containsKey(MESSAGE_KEY_CHANNEL_ID)) {
                return
            }

            val message = EncryptedPushMessage(
                channelId = it.getStringWithException(MESSAGE_KEY_CHANNEL_ID),
                encoding = it.getString(MESSAGE_KEY_ENCODING),
                body = it.getString(MESSAGE_KEY_BODY),
                salt = it.getString(MESSAGE_KEY_SALT),
                cryptoKey = it.getString(MESSAGE_KEY_CRYPTO_KEY)
            )

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

    override fun stop() {
        stopSelf()
    }

    override fun isServiceAvailable(context: Context): Boolean {
        // This is a strange API: the ADM SDK may exist on the device if it's an Amazon device, but we
        // also need to check if it's a *supported* Amazon device then.
        // We create a temporary instance of the ADM class, if it's available so that we can call `isSupported`.
        // This is the recommended approach as per the docs:
        // https://developer.amazon.com/docs/adm/integrate-your-app.html#gracefully-degrade-if-adm-is-unavailable
        return try {
            Class.forName("com.amazon.device.messaging.ADM")
            ADM(context).isSupported
        } catch (e: ClassNotFoundException) {
            logger.warn("ADM is not available on this device.")
            false
        }
    }

    private fun Bundle.getStringWithException(key: String): String {
        return getString(key) ?: throw NoSuchElementException(key)
    }
}
