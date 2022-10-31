/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.gecko.webnotifications

import mozilla.components.concept.engine.webnotifications.WebNotification
import mozilla.components.concept.engine.webnotifications.WebNotificationDelegate
import org.mozilla.geckoview.WebNotification as GeckoViewWebNotification
import org.mozilla.geckoview.WebNotificationDelegate as GeckoViewWebNotificationDelegate

internal class GeckoWebNotificationDelegate(
    private val webNotificationDelegate: WebNotificationDelegate,
) : GeckoViewWebNotificationDelegate {
    override fun onShowNotification(webNotification: GeckoViewWebNotification) {
        webNotificationDelegate.onShowNotification(webNotification.toWebNotification())
    }

    override fun onCloseNotification(webNotification: GeckoViewWebNotification) {
        webNotificationDelegate.onCloseNotification(webNotification.toWebNotification())
    }

    private fun GeckoViewWebNotification.toWebNotification(): WebNotification {
        return WebNotification(
            title = title,
            tag = tag,
            body = text,
            sourceUrl = source,
            iconUrl = imageUrl,
            direction = textDirection,
            lang = lang,
            requireInteraction = requireInteraction,
            triggeredByWebExtension = source == null,
            engineNotification = this@toWebNotification,
        )
    }
}
