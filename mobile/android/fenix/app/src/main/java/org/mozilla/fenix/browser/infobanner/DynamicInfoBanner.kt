/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.browser.infobanner

import android.content.Context
import android.view.ViewGroup
import androidx.annotation.VisibleForTesting
import androidx.coordinatorlayout.widget.CoordinatorLayout

/**
 * [InfoBanner] that will automatically scroll with the top [BrowserToolbar].
 * Only to be used with [BrowserToolbar]s placed at the top of the screen.
 *
 * @param shouldScrollWithTopToolbar whether to follow the Y translation of the top toolbar or not
 */
@Suppress("LongParameterList")
class DynamicInfoBanner(
    private val context: Context,
    container: ViewGroup,
    @VisibleForTesting
    internal val shouldScrollWithTopToolbar: Boolean = false,
    message: String,
    dismissText: String,
    actionText: String? = null,
    dismissByHiding: Boolean = false,
    dismissAction: (() -> Unit)? = null,
    actionToPerform: (() -> Unit)? = null
) : InfoBanner(
    context, container, message, dismissText, actionText, dismissByHiding, dismissAction, actionToPerform
) {
    override fun showBanner() {
        super.showBanner()

        if (shouldScrollWithTopToolbar) {
            (binding.root.layoutParams as CoordinatorLayout.LayoutParams).behavior = DynamicInfoBannerBehavior(
                context, null
            )
        }
    }
}
