/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.gecko.webpush

import mozilla.components.concept.engine.webpush.WebPushDelegate
import mozilla.components.concept.engine.webpush.WebPushSubscription
import org.mozilla.geckoview.GeckoResult
import org.mozilla.geckoview.WebPushDelegate as GeckoViewWebPushDelegate
import org.mozilla.geckoview.WebPushSubscription as GeckoWebPushSubscription

/**
 * A wrapper for the [WebPushDelegate] to communicate with the Gecko-based delegate.
 */
internal class GeckoWebPushDelegate(private val delegate: WebPushDelegate) : GeckoViewWebPushDelegate {

    /**
     * See [GeckoViewWebPushDelegate.onGetSubscription].
     */
    override fun onGetSubscription(scope: String): GeckoResult<GeckoWebPushSubscription>? {
        val result: GeckoResult<GeckoWebPushSubscription> = GeckoResult()

        delegate.onGetSubscription(scope) { subscription ->
            result.complete(subscription?.toGeckoSubscription())
        }

        return result
    }

    /**
     * See [GeckoViewWebPushDelegate.onSubscribe].
     */
    override fun onSubscribe(scope: String, appServerKey: ByteArray?): GeckoResult<GeckoWebPushSubscription>? {
        val result: GeckoResult<GeckoWebPushSubscription> = GeckoResult()

        delegate.onSubscribe(scope, appServerKey) { subscription ->
            result.complete(subscription?.toGeckoSubscription())
        }

        return result
    }

    /**
     * See [GeckoViewWebPushDelegate.onUnsubscribe].
     */
    override fun onUnsubscribe(scope: String): GeckoResult<Void>? {
        val result: GeckoResult<Void> = GeckoResult()

        delegate.onUnsubscribe(scope) { success ->
            if (success) {
                result.complete(null)
            } else {
                result.completeExceptionally(WebPushException("Un-subscribing from subscription failed."))
            }
        }

        return result
    }
}

/**
 * A helper extension to convert the subscription data class to the Gecko-based implementation.
 */
internal fun WebPushSubscription.toGeckoSubscription() = GeckoWebPushSubscription(
    scope,
    endpoint,
    appServerKey,
    publicKey,
    authSecret,
)

internal class WebPushException(message: String) : IllegalStateException(message)
