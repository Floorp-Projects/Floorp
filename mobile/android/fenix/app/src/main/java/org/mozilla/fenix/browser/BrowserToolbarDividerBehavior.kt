/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.browser

import android.content.Context
import android.util.AttributeSet
import android.view.View
import androidx.coordinatorlayout.widget.CoordinatorLayout
import mozilla.components.browser.toolbar.BrowserToolbar

/**
 * [CoordinatorLayout.Behavior] that will always position the [View] above the [BrowserToolbar] (including
 * when the browser toolbar is scrolling or performing a snap animation).
 */
@Suppress("unused") // Referenced from XML
class BrowserToolbarDividerBehavior(
    context: Context,
    attrs: AttributeSet
) : CoordinatorLayout.Behavior<View>(context, attrs) {
    override fun layoutDependsOn(parent: CoordinatorLayout, child: View, dependency: View): Boolean {
        if (dependency is BrowserToolbar) {
            return true
        }

        return super.layoutDependsOn(parent, child, dependency)
    }

    override fun onDependentViewChanged(parent: CoordinatorLayout, child: View, dependency: View): Boolean {
        return if (dependency is BrowserToolbar) {
            repositionView(child, dependency)
            true
        } else {
            false
        }
    }

    private fun repositionView(view: View, toolbar: BrowserToolbar) {
        view.translationY = (toolbar.translationY + toolbar.height * -1.0).toFloat()
    }
}
