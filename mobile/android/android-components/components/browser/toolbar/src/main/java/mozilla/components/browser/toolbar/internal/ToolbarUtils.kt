/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.toolbar.internal

import android.transition.TransitionManager
import android.view.View
import android.view.ViewGroup
import mozilla.components.browser.toolbar.display.removeLeftPaddingIfNeeded
import mozilla.components.concept.toolbar.Toolbar

internal fun ViewGroup.wrapAction(action: Toolbar.Action): ActionWrapper {
    val wrapper = ActionWrapper(action)

    if (action.visible()) {
        action.createView(this).let {
            wrapper.view = it
            addView(it)
        }
    }

    return wrapper
}

/**
 * Measures a list of actions and returns the needed width.
 */
internal fun measureActions(actions: List<ActionWrapper>, size: Int): Int {
    val sizeSpec = View.MeasureSpec.makeMeasureSpec(size, View.MeasureSpec.EXACTLY)

    return actions
        .asSequence()
        .mapNotNull { it.view }
        .mapIndexed { index, view ->
            val widthSpec = if (view.minimumWidth > size) {
                View.MeasureSpec.makeMeasureSpec(view.minimumWidth, View.MeasureSpec.EXACTLY)
            } else {
                sizeSpec
            }
            view.removeLeftPaddingIfNeeded(index != 0)
            view.measure(widthSpec, sizeSpec)
            size
        }
        .sum()
}

/**
 * Invalidate the given list of actions and remove or create views as needed.
 */
internal fun ViewGroup.invalidateActions(actions: List<ActionWrapper>) {
    TransitionManager.beginDelayedTransition(this)

    for (action in actions) {
        val visible = action.actual.visible()

        if (!visible && action.view != null) {
            // Action should not be visible anymore. Remove view.
            removeView(action.view)
            action.view = null
        } else if (visible && action.view == null) {
            // Action should be visible. Add view for it.
            action.actual.createView(this).let {
                action.view = it
                addView(it)
            }
        }

        action.view?.let { action.actual.bind(it) }
    }
}
