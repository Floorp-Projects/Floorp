/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.toolbar

import android.content.Context
import android.util.AttributeSet
import android.view.View
import androidx.coordinatorlayout.widget.CoordinatorLayout
import com.google.android.material.appbar.AppBarLayout
import kotlinx.coroutines.ExperimentalCoroutinesApi
import mozilla.components.concept.engine.EngineView
import mozilla.components.concept.engine.EngineView.InputResult.INPUT_RESULT_UNHANDLED
import org.mozilla.fenix.ext.settings

/**
 * ScrollingViewBehavior that will setScrollFlags on BrowserToolbar based on EngineView touch handling
 */
@ExperimentalCoroutinesApi
class SwipeRefreshScrollingViewBehavior(
    context: Context,
    attrs: AttributeSet?,
    private val engineView: EngineView,
    private val browserToolbarView: BrowserToolbarView
) : AppBarLayout.ScrollingViewBehavior(context, attrs) {
    override fun onStartNestedScroll(
        coordinatorLayout: CoordinatorLayout,
        child: View,
        directTargetChild: View,
        target: View,
        axes: Int,
        type: Int
    ): Boolean {

        if (!browserToolbarView.view.context.settings().shouldUseBottomToolbar) {
            val shouldDisable = engineView.getInputResult() == INPUT_RESULT_UNHANDLED
            browserToolbarView.setScrollFlags(shouldDisable)
        }

        return super.onStartNestedScroll(
            coordinatorLayout,
            child,
            directTargetChild,
            target,
            axes,
            type
        )
    }
}
