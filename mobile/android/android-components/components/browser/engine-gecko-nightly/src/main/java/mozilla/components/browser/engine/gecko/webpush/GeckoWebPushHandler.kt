/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.gecko.webpush

import mozilla.components.concept.engine.webpush.WebPushHandler
import mozilla.components.concept.engine.webpush.WebPushSubscription
import org.mozilla.geckoview.GeckoRuntime

/**
 * Gecko-based implementation of [WebPushHandler], wrapping the
 * controller object provided by GeckoView.
 */
internal class GeckoWebPushHandler(
    private val runtime: GeckoRuntime
) : WebPushHandler {

    /**
     * See [WebPushHandler].
     */
    override fun onPushMessage(subscription: WebPushSubscription, message: ByteArray?) {
        runtime.webPushController.onPushEvent(subscription.toGeckoSubscription(), message)
    }

    /**
     * See [WebPushHandler].
     */
    override fun onSubscriptionChanged(subscription: WebPushSubscription) {
        runtime.webPushController.onSubscriptionChanged(subscription.toGeckoSubscription())
    }
}
