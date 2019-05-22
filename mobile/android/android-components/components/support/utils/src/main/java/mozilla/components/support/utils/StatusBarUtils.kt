/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.utils

import android.view.View

object StatusBarUtils {
    private var statusBarSize = -1

    /**
     * Determine the height of the status bar asynchronously.
     */
    @Suppress("unused")
    fun getStatusBarHeight(view: View, block: (Int) -> Unit) {
        if (statusBarSize > 0) {
            block(statusBarSize)
        } else {
            view.setOnApplyWindowInsetsListener { _, insets ->
                statusBarSize = insets.systemWindowInsetTop
                block(statusBarSize)
                view.setOnApplyWindowInsetsListener(null)
                insets
            }
        }
    }
}
