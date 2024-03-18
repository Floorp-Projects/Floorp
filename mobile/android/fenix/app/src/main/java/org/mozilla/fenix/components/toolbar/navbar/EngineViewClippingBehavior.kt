/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.toolbar.navbar

import android.content.Context
import android.util.AttributeSet
import android.view.View
import androidx.annotation.VisibleForTesting
import androidx.coordinatorlayout.widget.CoordinatorLayout
import mozilla.components.browser.toolbar.BrowserToolbar
import mozilla.components.concept.engine.EngineView
import mozilla.components.concept.toolbar.ScrollableToolbar
import mozilla.components.support.ktx.android.view.findViewInHierarchy

/**
 * A modification of [mozilla.components.ui.widgets.behavior.EngineViewClippingBehavior] that supports two toolbars.
 *
 * This behavior adjusts the top margin of the [EngineView] parent to ensure that tab content is displayed
 * right below the top toolbar when it is translating upwards. Additionally, it modifies the
 * [EngineView.setVerticalClipping] when the bottom toolbar is translating downwards, ensuring that page content, like
 * banners or webpage toolbars, is displayed right above the app toolbar.
 *
 * This class could be a candidate to replace the original and be integrated into A-C:
 * https://bugzilla.mozilla.org/show_bug.cgi?id=1884835
 *
 * @param context [Context] for various Android interactions.
 * @param attrs XML attributes configuring this behavior.
 * @param engineViewParent The parent [View] of the [EngineView].
 * @param topToolbarHeight The height of [ScrollableToolbar] when placed above the [EngineView].
 */

class EngineViewClippingBehavior(
    context: Context?,
    attrs: AttributeSet?,
    private val engineViewParent: View,
    private val topToolbarHeight: Int,
) : CoordinatorLayout.Behavior<View>(context, attrs) {

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal var engineView = engineViewParent.findViewInHierarchy { it is EngineView } as EngineView?

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal var recentBottomToolbarTranslation = 0f

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal var recentTopToolbarTranslation = 0f

    private val hasTopToolbar = topToolbarHeight > 0

    override fun layoutDependsOn(parent: CoordinatorLayout, child: View, dependency: View): Boolean {
        if (dependency is ScrollableToolbar) {
            return true
        }

        return super.layoutDependsOn(parent, child, dependency)
    }

    // This method will be sequentially called with BrowserToolbar and ToolbarContainerView as dependencies when the
    // navbar feature is on. Each call adjusts the translations of both elements and saves the most recent ones for
    // future calls, ensuring that translations remain in sync. This is crucial, especially in cases where the toolbars
    // have different sizes: as the top toolbar moves up, the bottom content clipping should be adjusted at twice the
    // speed to compensate for the increased parent view height. However, once the top toolbar is completely hidden, the
    // bottom content clipping should then move at the normal speed.
    override fun onDependentViewChanged(parent: CoordinatorLayout, child: View, dependency: View): Boolean {
        // Added NaN check for translationY as a precaution based on historical issues observed in
        // [https://bugzilla.mozilla.org/show_bug.cgi?id=1823306]. This check aims to prevent similar issues, as
        // confirmed by the test. Further investigation might be needed to identify all possible causes of NaN values.
        if (dependency.translationY.isNaN()) {
            return true
        }

        when (dependency) {
            is BrowserToolbar -> {
                if (hasTopToolbar) {
                    recentTopToolbarTranslation = dependency.translationY
                } else {
                    recentBottomToolbarTranslation = dependency.translationY
                }
            }
            is ToolbarContainerView -> recentBottomToolbarTranslation = dependency.translationY
        }

        engineView?.let {
            if (hasTopToolbar) {
                // Here we are adjusting the vertical position of
                // the engine view container to be directly under
                // the toolbar. The top toolbar is shifting up, so
                // its translation will be either negative or zero.
                // It might be safe to use the child view here, but the original
                // implementation was adjusting the size of the parent passed
                // to the class directly, and I feel cautious to change that
                // considering possible side effects.
                engineViewParent.translationY = recentTopToolbarTranslation + topToolbarHeight
            }

            // We want to position the engine view popup content
            // right above the bottom toolbar when the toolbar
            // is being shifted down. The top of the bottom toolbar
            // is either positive or zero, but for clipping
            // the values should be negative because the baseline
            // for clipping is bottom toolbar height.
            val contentBottomClipping = recentTopToolbarTranslation - recentBottomToolbarTranslation
            it.setVerticalClipping(contentBottomClipping.toInt())
        }
        return true
    }
}
