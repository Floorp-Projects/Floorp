/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.widget

import android.animation.Animator
import android.animation.AnimatorListenerAdapter
import android.content.Context
import android.util.AttributeSet
import android.view.View
import androidx.coordinatorlayout.widget.CoordinatorLayout
import com.google.android.material.floatingactionbutton.FloatingActionButton
import mozilla.components.browser.toolbar.BrowserToolbar
import kotlin.math.absoluteValue
import kotlin.math.roundToInt

private const val ANIMATION_DURATION = 300L

/**
 * A Behavior implementation that will hide/show a FloatingActionButton based on whether a [BrowserToolbar]
 * is visible or not.
 */
class FloatingActionButtonBehavior(
    context: Context?,
    attrs: AttributeSet?
) : CoordinatorLayout.Behavior<FloatingActionButton>(context, attrs) {
    /**
     * Controls whether the dynamic behavior depending on toolbar changes is enabled or not.
     */
    var autoHideEnabled = true

    private var isVisible = true
    private var button: FloatingActionButton? = null

    override fun layoutDependsOn(parent: CoordinatorLayout, child: FloatingActionButton, dependency: View): Boolean {
        if (button !== child) {
            button = child
        }

        if (dependency::class == BrowserToolbar::class) {
            return true
        }

        return super.layoutDependsOn(parent, child, dependency)
    }

    @Suppress("ReturnCount")
    override fun onDependentViewChanged(parent: CoordinatorLayout, fab: FloatingActionButton, toolbar: View): Boolean {
        if (!autoHideEnabled) {
            return false
        }

        button?.let {
            if (!isVisible && toolbar.translationY.roundToInt() == 0) {
                showButton(it)
                return true
            } else if (isVisible && toolbar.translationY.roundToInt().absoluteValue == toolbar.height) {
                hideButton(it)
                return true
            }
        }

        return false
    }

    private fun showButton(fab: FloatingActionButton) {
        animate(fab, false)
    }

    private fun hideButton(fab: FloatingActionButton) {
        animate(fab, true)
    }

    private fun animate(fab: FloatingActionButton, hide: Boolean) {
        // // Ensure the child will be visible before starting animation: if it's hidden, we've also
        // // set it to View.GONE, so we need to restore that now, _before_ the animation starts.
        if (!hide) {
            fab.visibility = View.VISIBLE
        }

        fab.animate()
            .scaleX(if (hide) 0f else 1f)
            .scaleY(if (hide) 0f else 1f)
            .setDuration(ANIMATION_DURATION)
            .setListener(object : AnimatorListenerAdapter() {
                override fun onAnimationEnd(animation: Animator) {
                    isVisible = !hide

                    // Hide the FAB: even when it has size=0x0, it still intercept click events,
                    // so we get phantom clicks causing focus to erase if the user presses
                    // near where the FAB would usually be shown.
                    if (hide) {
                        fab.visibility = View.GONE
                    }
                }
            })
            .start()
    }
}
