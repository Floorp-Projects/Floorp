/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.gecko.webpush

import mozilla.components.concept.engine.webpush.WebPushHandler
import org.mozilla.geckoview.GeckoRuntime

/**
 * Gecko-based implementation of [WebPushHandler], wrapping the
 * controller object provided by GeckoView.
 */
internal class GeckoWebPushHandler(
    private val runtime: GeckoRuntime,
) : WebPushHandler {

    /**
     * See [WebPushHandler].
     */
    override fun onPushMessage(scope: String, message: ByteArray?) {
        runtime.webPushController.onPushEvent(scope, message)
    }

    /**
     * See [WebPushHandler].
     */
    override fun onSubscriptionChanged(scope: String) {
        runtime.webPushController.onSubscriptionChanged(scope)
    }
}
