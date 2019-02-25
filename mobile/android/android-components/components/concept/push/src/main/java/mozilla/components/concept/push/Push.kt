/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 *  License, v. 2.0. If a copy of the MPL was not distributed with this
 *  file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

package mozilla.components.concept.push

/**
 * The Push component that will handle push messages from the [PushService] provided.
 * Starting Push in the Application's onCreate is recommended.
 */
class Push(
    internal val service: PushService
) {
    /**
     * Start the Push component.
     */
    fun init(): Push {
        Push.instance = this

        service.start()

        return this
    }

    internal fun onNewToken(newToken: String) {}

    internal fun onMessageReceived(message: String /*PushMessage*/) {}

    internal fun onError(error: Error) {}

    companion object {
        private var instance: Push? = null
        internal val requireInstance: Push
            get() = instance ?: throw IllegalStateException(
                "You need to call init() on your Push instance from Application.onCreate()."
            )
    }
}

sealed class Error {
    data class RegistrationError(val desc: String) : Error()
    data class NetworkError(val desc: String) : Error()
}
