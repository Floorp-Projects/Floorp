/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.engine.webpush

import mozilla.components.concept.engine.Engine

/**
 * A handler for all WebPush messages and [subscriptions][0] to be delivered to the [Engine].
 *
 * [0]: https://developer.mozilla.org/en-US/docs/Web/API/PushSubscription
 */
interface WebPushHandler {

    /**
     * Invoked when a push message has been delivered.
     *
     * @param scope The subscription identifier which usually represents the website's URI.
     * @param message A [ByteArray] message.
     */
    fun onPushMessage(scope: String, message: ByteArray?)

    /**
     * Invoked when a subscription has now changed/expired.
     */
    fun onSubscriptionChanged(scope: String) = Unit
}

/**
 * A data class representation of the [PushSubscription][0] web specification.
 *
 * [0]: https://developer.mozilla.org/en-US/docs/Web/API/PushSubscription
 *
 * @param scope The subscription identifier which usually represents the website's URI.
 * @param endpoint The Web Push endpoint for this subscription.
 * This is the URL of a web service which implements the Web Push protocol.
 * @param appServerKey A public key a server will use to send messages to client apps via a push server.
 * @param publicKey The public key generated, to be provided to the app server for message encryption.
 * @param authSecret A secret key generated, to be provided to the app server for use in encrypting
 * and authenticating messages sent to the endpoint.
 */
data class WebPushSubscription(
    val scope: String,
    val endpoint: String,
    val appServerKey: ByteArray?,
    val publicKey: ByteArray,
    val authSecret: ByteArray,
) {
    @Suppress("ComplexMethod")
    override fun equals(other: Any?): Boolean {
        /* auto-generated */
        if (this === other) return true
        if (javaClass != other?.javaClass) return false

        other as WebPushSubscription

        if (scope != other.scope) return false
        if (endpoint != other.endpoint) return false
        if (appServerKey != null) {
            if (other.appServerKey == null) return false
            if (!appServerKey.contentEquals(other.appServerKey)) return false
        } else if (other.appServerKey != null) return false
        if (!publicKey.contentEquals(other.publicKey)) return false
        if (!authSecret.contentEquals(other.authSecret)) return false

        return true
    }

    @Suppress("MagicNumber")
    override fun hashCode(): Int {
        /* auto-generated */
        var result = scope.hashCode()
        result = 31 * result + endpoint.hashCode()
        result = 31 * result + (appServerKey?.contentHashCode() ?: 0)
        result = 31 * result + publicKey.contentHashCode()
        result = 31 * result + authSecret.contentHashCode()
        return result
    }
}
