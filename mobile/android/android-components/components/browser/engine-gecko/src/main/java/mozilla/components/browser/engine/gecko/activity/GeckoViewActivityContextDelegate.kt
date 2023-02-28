/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.gecko.activity

import android.app.Activity
import android.content.Context
import mozilla.components.support.base.log.logger.Logger
import org.mozilla.geckoview.GeckoView
import java.lang.ref.WeakReference

/**
 * GeckoViewActivityContextDelegate provides an active Activity to GeckoView or null. Not to be confused
 * with the runtime delegate of [GeckoActivityDelegate], which is tightly coupled to webauthn.
 * See bug 1806191 for more information on delegate differences.
 *
 * @param contextRef A reference to an active Activity context or null for GeckoView to use.
 */
class GeckoViewActivityContextDelegate(
    private val contextRef: WeakReference<Context?>,
) : GeckoView.ActivityContextDelegate {
    private val logger = Logger("GeckoViewActivityContextDelegate")
    init {
        if (contextRef.get() == null) {
            logger.warn("Activity context is null.")
        } else if (contextRef.get() !is Activity) {
            logger.warn("A non-activity context was set.")
        }
    }

    /**
     * Used by GeckoView to get an Activity context for operations such as printing.
     * @return An active Activity context or null.
     */
    override fun getActivityContext(): Context? {
        val context = contextRef.get()
        if ((context == null)) {
            return null
        }
        return context
    }
}
