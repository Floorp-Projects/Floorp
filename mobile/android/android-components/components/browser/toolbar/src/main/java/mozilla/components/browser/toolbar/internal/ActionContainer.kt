/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.toolbar.internal

import android.content.Context
import android.transition.TransitionManager
import android.util.AttributeSet
import android.view.Gravity
import android.view.View
import android.widget.LinearLayout
import androidx.annotation.VisibleForTesting
import androidx.core.view.isVisible
import mozilla.components.browser.toolbar.R
import mozilla.components.concept.toolbar.Toolbar

/**
 * A container [View] for displaying [Toolbar.Action] objects.
 */
internal class ActionContainer @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = 0,
) : LinearLayout(context, attrs, defStyleAttr) {
    private val actions = mutableListOf<ActionWrapper>()
    private var actionSize: Int? = null

    init {
        gravity = Gravity.CENTER_VERTICAL
        orientation = HORIZONTAL
        visibility = View.GONE

        context.obtainStyledAttributes(
            attrs,
            R.styleable.ActionContainer,
            defStyleAttr,
            0,
        ).run {
            actionSize = attrs?.let {
                getDimensionPixelSize(R.styleable.ActionContainer_actionContainerItemSize, 0)
            }

            recycle()
        }
    }

    fun addAction(action: Toolbar.Action) {
        val wrapper = ActionWrapper(action)

        if (action.visible()) {
            visibility = View.VISIBLE

            action.createView(this).let {
                wrapper.view = it
                val insertionIndex = calculateInsertionIndex(action)
                addActionView(it, insertionIndex)
            }
        }

        actions.add(wrapper)
    }

    /**
     * Essentially calculates the index of an action on toolbar based on a
     * map [visibleActionIndicesWithWeights] that holds the order
     * of visible action indices to their weights sorted by weights.
     * An index is now calculated by finding the immediate next larger weight
     * compared to the new action's weight. Index of this find becomes the index of the new action.
     * If not found, action is appended at the end.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal fun calculateInsertionIndex(newAction: Toolbar.Action): Int {
        if (newAction.weight() == -1) {
            return -1
        }
        val visibleActionIndicesWithWeights = actions.filter { it.actual.visible() }
            .mapNotNull { actionWrapper ->
                val index = indexOfChild(actionWrapper.view)
                if (index != -1) index to actionWrapper.actual.weight() else null
            }.sortedBy { it.second }

        val insertionIndex = visibleActionIndicesWithWeights.firstOrNull { it.second > newAction.weight() }?.first

        return insertionIndex ?: childCount
    }

    fun removeAction(action: Toolbar.Action) {
        actions.find { it.actual == action }?.let {
            actions.remove(it)
            removeView(it.view)
        }
    }

    fun invalidateActions() {
        TransitionManager.beginDelayedTransition(this)
        var updatedVisibility = View.GONE

        for (action in actions) {
            val visible = action.actual.visible()

            if (visible) {
                updatedVisibility = View.VISIBLE
            }

            if (!visible && action.view != null) {
                removeView(action.view)
                action.view = null
            } else if (visible && action.view == null) {
                action.actual.createView(this).let {
                    action.view = it
                    val insertionIndex = calculateInsertionIndex(action.actual)
                    addActionView(it, insertionIndex)
                }
            }

            action.view?.let { action.actual.bind(it) }
        }

        visibility = updatedVisibility
    }

    fun autoHideAction(isVisible: Boolean) {
        for (action in actions) {
            if (action.actual.autoHide()) {
                action.view?.isVisible = isVisible
            }
        }
    }

    private fun addActionView(view: View, index: Int) {
        addView(view, index, LayoutParams(actionSize ?: 0, actionSize ?: 0))
    }
}
