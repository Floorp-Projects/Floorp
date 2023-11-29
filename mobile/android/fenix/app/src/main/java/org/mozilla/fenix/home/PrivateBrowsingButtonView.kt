/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home

import android.view.View
import androidx.annotation.StringRes
import org.mozilla.fenix.R
import org.mozilla.fenix.browser.browsingmode.BrowsingMode

/**
 * Sets up the private browsing toggle button on the [HomeFragment].
 *
 * @param button The button to bind content descriptions and click listeners to.
 * @param mode The current [BrowsingMode].
 * @param onClick Click handler for the button.
 */
class PrivateBrowsingButtonView(
    button: View,
    private val mode: BrowsingMode,
    private val onClick: (BrowsingMode) -> Unit,
) : View.OnClickListener {

    init {
        button.contentDescription = button.context.getString(getContentDescription(mode))
        button.setOnClickListener(this)
    }

    /**
     * Calls [onClick] with the new [BrowsingMode] and updates the [browsingModeManager].
     */
    override fun onClick(v: View) {
        onClick(mode.inverted)
    }

    companion object {

        /**
         * Returns the appropriate content description depending on the browsing mode.
         */
        @StringRes
        private fun getContentDescription(mode: BrowsingMode) = when (mode) {
            BrowsingMode.Normal -> R.string.content_description_private_browsing_button
            BrowsingMode.Private -> R.string.content_description_disable_private_browsing_button
        }
    }
}
