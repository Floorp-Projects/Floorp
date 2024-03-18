/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray.browser

import android.content.Context
import android.graphics.PointF
import android.graphics.Rect
import android.util.AttributeSet
import android.view.DragEvent
import android.view.View
import androidx.recyclerview.widget.ItemTouchHelper
import androidx.recyclerview.widget.RecyclerView
import mozilla.components.browser.tabstray.TabViewHolder
import org.mozilla.fenix.tabstray.TabsTrayInteractor
import org.mozilla.fenix.tabstray.TabsTrayStore
import kotlin.math.abs

/**
 * The base class for a tabs tray list that wants to display browser tabs.
 */
abstract class AbstractBrowserTrayList @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = 0,
) : RecyclerView(context, attrs, defStyleAttr) {

    lateinit var interactor: TabsTrayInteractor
    lateinit var tabsTrayStore: TabsTrayStore

    private var lastDragPos: PointF? = null
    private var lastDragData: TabDragData? = null

    protected val swipeToDelete by lazy {
        SwipeToDeleteBinding(tabsTrayStore)
    }

    override fun onAttachedToWindow() {
        super.onAttachedToWindow()

        swipeToDelete.start()

        adapter?.onAttachedToRecyclerView(this)
        this.setOnDragListener(dragListen)
        itemAnimator = DraggableItemAnimator()
    }

    override fun onDetachedFromWindow() {
        super.onDetachedFromWindow()

        swipeToDelete.stop()

        // Notify the adapter that it is released from the view preemptively.
        adapter?.onDetachedFromRecyclerView(this)
        this.setOnDragListener(null)
    }

    // Find the closest item to the x/y position of the drop.
    private data class DropPositionData(val id: String, val placeAfter: Boolean, val view: View)
    private fun getDropPosition(x: Float, y: Float, source: String): DropPositionData? {
        if (childCount < 2) return null // If there's 0 or 1 tabs visible, can't reorder
        var bestDist = Float.MAX_VALUE
        var bestOut: DropPositionData? = null
        var seenSource = false
        for (i in 0 until childCount) {
            val proposedTarget = getChildAt(i)
            val targetHolder = findContainingViewHolder(proposedTarget)
            if (targetHolder is TabViewHolder) {
                val rect = Rect() // Get post-animation positioning
                getDecoratedBoundsWithMargins(proposedTarget, rect)
                val targetX = (rect.left + rect.right) / 2
                val targetY = (rect.top + rect.bottom) / 2
                val xDiff = x - targetX
                val yDiff = y - targetY
                val dist = abs(xDiff) + abs(yDiff)
                val id = targetHolder.tab?.id
                // Determine before/after drop placement
                // based on if source tab is coming from before/after the target
                if (id == source) seenSource = true
                if (dist < bestDist && id != null) {
                    bestDist = dist
                    bestOut = DropPositionData(id, seenSource, proposedTarget)
                }
            }
        }
        return bestOut
    }
    private fun findSourceViewAndHolder(id: String): Pair<View, AbstractBrowserTabViewHolder>? {
        for (i in 0 until childCount) {
            val proposed = getChildAt(i)
            val targetHolder = findContainingViewHolder(proposed)
            if (targetHolder is AbstractBrowserTabViewHolder && targetHolder.tab?.id == id) {
                return Pair(proposed, targetHolder)
            }
        }
        return null
    }
    private val dragListen = OnDragListener { _, event ->
        if (event.localState is TabDragData) {
            val (tab, _) = event.localState as TabDragData
            val sourceId = tab.id
            val sources = findSourceViewAndHolder(sourceId)

            when (event.action) {
                DragEvent.ACTION_DRAG_STARTED -> {
                    // Put the dragged tab on top of all other tabs
                    if (sources != null) {
                        val (sourceView, sourceViewHolder) = sources
                        sourceViewHolder.beingDragged = true
                        sourceView.elevation = DRAGGED_TAB_ELEVATION
                    }
                    // Setup the scrolling/updating loop
                    lastDragPos = PointF(event.x, event.y)
                    lastDragData = event.localState as TabDragData
                    handler.postDelayed(dragRunnable, DRAG_UPDATE_PERIOD_MS)
                    true
                }
                DragEvent.ACTION_DRAG_ENTERED -> {
                    true
                }
                DragEvent.ACTION_DRAG_LOCATION -> {
                    lastDragPos = PointF(event.x, event.y)
                    true
                }
                DragEvent.ACTION_DRAG_EXITED -> {
                    true
                }
                DragEvent.ACTION_DROP -> {
                    true
                }
                DragEvent.ACTION_DRAG_ENDED -> {
                    // Move tab to center, set dragging to false, return tab to normal height
                    if (sources != null) {
                        val (sourceView, sourceViewHolder) = sources
                        sourceViewHolder.beingDragged = false
                        sourceView.elevation = 0f
                        sourceView.animate()
                            .translationX(0f).translationY(0f).duration =
                            itemAnimator?.moveDuration ?: 0
                    }
                    // This will stop the scroll/update loop
                    lastDragPos = null
                    lastDragData = null
                    true
                }
                else -> { // Unknown action
                    false
                }
            }
        } else {
            false
        }
    }

    private val dragRunnable: Runnable = object : Runnable {
        override fun run() {
            val pos = lastDragPos
            val data = lastDragData
            if (pos == null || data == null) return
            val (tab, dragOffset) = data
            val sourceId = tab.id
            val sources = findSourceViewAndHolder(sourceId)
            // Move the tab's visual position
            if (sources != null) {
                val (sourceView, sourceViewHolder) = sources
                sourceView.x = pos.x - dragOffset.x
                sourceView.y = pos.y - dragOffset.y
                sourceViewHolder.beingDragged = true
                sourceView.elevation = DRAGGED_TAB_ELEVATION

                // Move the tab's position in the list
                val target = getDropPosition(pos.x, pos.y, tab.id)
                if (target != null) {
                    val (targetId, placeAfter, targetView) = target
                    if (sourceView != targetView) {
                        interactor.onTabsMove(tab.id, targetId, placeAfter)
                        // Deal with https://issuetracker.google.com/issues/37018279
                        // See also https://stackoverflow.com/questions/27992427
                        (layoutManager as? ItemTouchHelper.ViewDropHandler)?.prepareForDrop(
                            sourceView,
                            targetView,
                            sourceView.left,
                            sourceView.top,
                        )
                    }
                }
            }
            // Scroll the tray
            var scroll = 0
            if (pos.y < SCROLL_AREA) scroll = -SCROLL_SPEED
            if (pos.y > height - SCROLL_AREA) scroll = SCROLL_SPEED
            scrollBy(0, scroll)

            // Repeats forever, until lastDragPos/Data are null
            handler.postDelayed(this, DRAG_UPDATE_PERIOD_MS)
        }
    }
    companion object {
        internal const val DRAGGED_TAB_ELEVATION = 10f
        internal const val DRAG_UPDATE_PERIOD_MS = 10L
        internal const val SCROLL_SPEED = 20
        internal const val SCROLL_AREA = 200
    }
}
