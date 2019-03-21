/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 *  License, v. 2.0. If a copy of the MPL was not distributed with this
 *  file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

package mozilla.components.concept.push

import android.support.annotation.CallSuper

/**
 * A push notification processor that handles registration and new messages from the [PushService] provided.
 * Starting Push in the Application's onCreate is recommended.
 */
abstract class PushProcessor {

    /**
     * Initialize the PushProcessor before it
     */
    @CallSuper
    open fun initialize() {
        instance = this
    }

    /**
     * Start the push processor and starts any service associated.
     */
    abstract fun start()

    /**
     * Stop the push service and stops any service associated.
     */
    abstract fun stop()

    /**
     * A new registration token has been received.
     */
    abstract fun onNewToken(newToken: String)

    /**
     * A new push message has been received.
     */
    abstract fun onMessageReceived(message: PushMessage)

    /**
     * An error has occurred.
     */
    abstract fun onError(error: Error)

    companion object {
        @Volatile
        var instance: PushProcessor? = null
        val requireInstance: PushProcessor
            get() = instance ?: throw IllegalStateException(
                "You need to call start() on your Push instance from Application.onCreate()."
            )
    }
}

/**
 * A push message holds the information needed to pass the message on to the appropriate receiver.
 */
data class PushMessage(val data: String)

/**
 * The different kinds of push messages.
 */
enum class PushType {
    Services,
    ThirdParty
}

/**
 *  Various error types.
 */
sealed class Error {
    data class RegistrationError(val desc: String) : Error()
    data class NetworkError(val desc: String) : Error()
}
