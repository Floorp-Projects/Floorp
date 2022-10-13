/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.push

import androidx.annotation.VisibleForTesting

/**
 * A push notification processor that handles registration and new messages from the [PushService] provided.
 * Starting Push in the Application's onCreate is recommended.
 */
interface PushProcessor {

    /**
     * Start the push processor and any service associated.
     */
    fun initialize()

    /**
     * Removes all push subscriptions from the device.
     */
    fun shutdown()

    /**
     * A new registration token has been received.
     */
    fun onNewToken(newToken: String)

    /**
     * A new push message has been received.
     */
    fun onMessageReceived(message: EncryptedPushMessage)

    /**
     * An error has occurred.
     */
    fun onError(error: PushError)

    /**
     * Requests the [PushService] to renew it's registration with it's provider.
     */
    fun renewRegistration()

    companion object {
        /**
         * Initialize and installs the PushProcessor into the application.
         * This needs to be called in the application's onCreate before a push service has started.
         */
        fun install(processor: PushProcessor) {
            instance = processor
        }

        @Volatile
        private var instance: PushProcessor? = null

        @VisibleForTesting
        internal fun reset() {
            instance = null
        }
        val requireInstance: PushProcessor
            get() = instance ?: throw IllegalStateException(
                "You need to call PushProcessor.install() on your Push instance from Application.onCreate().",
            )
    }
}

/**
 * A push message holds the information needed to pass the message on to the appropriate receiver.
 */
data class EncryptedPushMessage(
    val channelId: String,
    val body: String?,
    val encoding: String,
    val salt: String = "",
    val cryptoKey: String = "", // diffie–hellman key
) {
    companion object {
        /**
         * The [salt] and [cryptoKey] are optional as part of the standard for WebPush, so we should default
         * to empty strings.
         *
         * Note: We have use the invoke operator since secondary constructors don't work with nullable primitive types.
         */
        operator fun invoke(
            channelId: String,
            body: String?,
            encoding: String?,
            salt: String? = null,
            cryptoKey: String? = null,
        ) = EncryptedPushMessage(channelId, body, encoding ?: "aes128gcm", salt ?: "", cryptoKey ?: "")
    }
}

/**
 *  Various error types.
 */
sealed class PushError(override val message: String) : Exception() {
    data class Registration(override val message: String) : PushError(message)
    data class Network(override val message: String) : PushError(message)

    /**
     * @property cause Original exception from Rust code.
     */
    data class Rust(
        override val cause: Throwable?,
        override val message: String = cause?.message.orEmpty(),
    ) : PushError(message)
    data class MalformedMessage(override val message: String) : PushError(message)
    data class ServiceUnavailable(override val message: String) : PushError(message)
}
