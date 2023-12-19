/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components

import android.content.Context
import android.view.Gravity
import mozilla.components.feature.downloads.DownloadsFeature
import org.mozilla.fenix.R
import org.mozilla.fenix.theme.ThemeManager

/**
 * Provides access to all Fenix download styling.
 */
object DownloadStyling {

    /**
     * creates [DownloadsFeature.PromptsStyling].
     */
    fun createPrompt(context: Context): DownloadsFeature.PromptsStyling {
        return DownloadsFeature.PromptsStyling(
            gravity = Gravity.BOTTOM,
            shouldWidthMatchParent = true,
            positiveButtonBackgroundColor = ThemeManager.resolveAttribute(
                R.attr.accent,
                context,
            ),
            positiveButtonTextColor = ThemeManager.resolveAttribute(
                R.attr.textOnColorPrimary,
                context,
            ),
            positiveButtonRadius = (context.resources.getDimensionPixelSize(R.dimen.tab_corner_radius)).toFloat(),
        )
    }
}
