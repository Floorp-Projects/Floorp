/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.browser.infobanner

import android.content.Context
import android.util.AttributeSet
import android.view.View
import androidx.annotation.VisibleForTesting
import androidx.coordinatorlayout.widget.CoordinatorLayout
import mozilla.components.browser.toolbar.BrowserToolbar

/**
 * A [CoordinatorLayout.Behavior] implementation to be used when placing [InfoBanner]
 * below the BrowserToolbar with which is has to scroll.
 *
 * This Behavior will keep the Y translations of [InfoBanner] and the top [BrowserToolbar] in sync
 * so that the banner will be shown between:
 * - the top of the container, being translated over the initial toolbar height (toolbar fully collapsed)
 * - immediately below the toolbar (toolbar fully expanded).
 */
class DynamicInfoBannerBehavior(
    context: Context?,
    attrs: AttributeSet?,
) : CoordinatorLayout.Behavior<View>(context, attrs) {
    @VisibleForTesting
    internal var toolbarHeight: Int = 0

    override fun layoutDependsOn(parent: CoordinatorLayout, child: View, dependency: View): Boolean {
        if (dependency::class == BrowserToolbar::class) {
            toolbarHeight = dependency.height
            setBannerYTranslation(child, dependency.translationY)
            return true
        }

        return super.layoutDependsOn(parent, child, dependency)
    }

    override fun onDependentViewChanged(parent: CoordinatorLayout, child: View, dependency: View): Boolean {
        setBannerYTranslation(child, dependency.translationY)

        return true
    }

    @VisibleForTesting
    internal fun setBannerYTranslation(banner: View, newYTranslation: Float) {
        banner.translationY = toolbarHeight + newYTranslation
    }
}
