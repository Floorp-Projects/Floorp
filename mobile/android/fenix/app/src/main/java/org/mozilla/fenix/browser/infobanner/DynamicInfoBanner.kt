/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.browser.infobanner

import android.content.Context
import android.view.ViewGroup
import androidx.coordinatorlayout.widget.CoordinatorLayout

/**
 * [InfoBanner] that will automatically scroll with the top [BrowserToolbar].
 * Only to be used with [BrowserToolbar]s placed at the top of the screen.
 *
 * @param context A [Context] for accessing system resources.
 * @param container The layout where the banner will be shown.
 * @property shouldScrollWithTopToolbar whether to follow the Y translation of the top toolbar or not.
 * @param message The message displayed in the banner.
 * @param dismissText The text on the dismiss button.
 * @param actionText The text on the action to perform button.
 * @param dismissByHiding Whether or not to hide the banner when dismissed.
 * @param dismissAction  Optional callback invoked when the user dismisses the banner.
 * @param actionToPerform The action to be performed on action button press.
 */
class DynamicInfoBanner(
    private val context: Context,
    container: ViewGroup,
    internal val shouldScrollWithTopToolbar: Boolean = false,
    message: String,
    dismissText: String,
    actionText: String? = null,
    dismissByHiding: Boolean = false,
    dismissAction: (() -> Unit)? = null,
    actionToPerform: (() -> Unit)? = null,
) : InfoBanner(
    context,
    container,
    message,
    dismissText,
    actionText,
    dismissByHiding,
    dismissAction,
    actionToPerform,
) {
    override fun showBanner() {
        super.showBanner()

        if (shouldScrollWithTopToolbar) {
            (binding.root.layoutParams as CoordinatorLayout.LayoutParams).behavior = DynamicInfoBannerBehavior(
                context,
                null,
            )
        }
    }
}
