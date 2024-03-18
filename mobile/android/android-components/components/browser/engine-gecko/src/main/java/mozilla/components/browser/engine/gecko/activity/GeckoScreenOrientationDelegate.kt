/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.gecko.activity

import mozilla.components.concept.engine.activity.OrientationDelegate
import org.mozilla.geckoview.AllowOrDeny
import org.mozilla.geckoview.AllowOrDeny.ALLOW
import org.mozilla.geckoview.AllowOrDeny.DENY
import org.mozilla.geckoview.GeckoResult
import org.mozilla.geckoview.OrientationController

/**
 * Default [OrientationController.OrientationDelegate] implementation that delegates both the behavior
 * and the returned value to a [OrientationDelegate].
 */
internal class GeckoScreenOrientationDelegate(
    private val delegate: OrientationDelegate,
) : OrientationController.OrientationDelegate {
    override fun onOrientationLock(requestedOrientation: Int): GeckoResult<AllowOrDeny> {
        val result = GeckoResult<AllowOrDeny>()

        when (delegate.onOrientationLock(requestedOrientation)) {
            true -> result.complete(ALLOW)
            false -> result.complete(DENY)
        }

        return result
    }

    override fun onOrientationUnlock() {
        delegate.onOrientationUnlock()
    }
}
