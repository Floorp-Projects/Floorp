/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.engine.webpush

/**
 * Notifies applications or other components of engine events related to Web Push notifications.
 */
interface WebPushDelegate {

    /**
     * Requests a WebPush subscription for the given Service Worker scope.
     */
    fun onGetSubscription(scope: String, onSubscription: (WebPushSubscription?) -> Unit) = Unit

    /**
     * Create a WebPush subscription for the given Service Worker scope.
     */
    fun onSubscribe(scope: String, serverKey: ByteArray?, onSubscribe: (WebPushSubscription?) -> Unit) = Unit

    /**
     * Remove a subscription for the given Service Worker scope.
     *
     * @return whether the unsubscribe was successful or not.
     */
    fun onUnsubscribe(scope: String, onUnsubscribe: (Boolean) -> Unit) = Unit
}
