/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.session.behavior

import android.annotation.SuppressLint
import android.content.Context
import android.util.AttributeSet
import android.view.View
import androidx.coordinatorlayout.widget.CoordinatorLayout
import mozilla.components.concept.engine.EngineView

/**
 * A [CoordinatorLayout.Behavior] implementation to be used with [EngineView] when placing a toolbar at the
 * bottom of the screen.
 *
 * Using this behavior requires the toolbar to use the BrowserToolbarBottomBehavior.
 *
 * This implementation will update the vertical clipping of the [EngineView] so that bottom-aligned web content will
 * be drawn above the native toolbar.
 */
class EngineViewBottomBehavior(
    context: Context?,
    attrs: AttributeSet?
) : CoordinatorLayout.Behavior<View>(context, attrs) {
    @SuppressLint("LogUsage")
    override fun layoutDependsOn(parent: CoordinatorLayout, child: View, dependency: View): Boolean {
        // This package does not have access to "BrowserToolbar" ... so we are just checking the class name here since
        // we actually do not need anything from that class - we only need to identify the instance.
        // Right now we do not have a component that has access to (concept/browser-toolbar and concept-engine).
        // Creating one just for this behavior is too excessive.
        if (dependency::class.java.simpleName == "BrowserToolbar") {
            return true
        }

        return super.layoutDependsOn(parent, child, dependency)
    }

    override fun onDependentViewChanged(parent: CoordinatorLayout, child: View, dependency: View): Boolean {
        (child as EngineView).setVerticalClipping(dependency.height - dependency.translationY.toInt())
        return true
    }
}
