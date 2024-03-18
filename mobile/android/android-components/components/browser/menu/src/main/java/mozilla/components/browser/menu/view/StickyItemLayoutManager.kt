/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.menu.view

import android.annotation.SuppressLint
import android.content.Context
import android.graphics.PointF
import android.os.Parcelable
import android.view.View
import android.view.ViewTreeObserver
import androidx.annotation.VisibleForTesting
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import kotlinx.parcelize.Parcelize

// Inspired from
// https://github.com/qiujayen/sticky-layoutmanager/blob/b1ddb086db5b04ff3c5357dabe1bff47a935dd37/
// sticky-layoutmanager/src/main/java/com/jay/widget/StickyHeadersLinearLayoutManager.java

/**
 * Contract needed to be implemented by all [RecyclerView.Adapter]s
 * that want to display a list with a sticky header / footer.
 */
interface StickyItemsAdapter {
    /**
     * Whether this should be considered a sticky item.
     *
     * All items will be checked. Only the last one presenting as sticky will be used as such.
     */
    fun isStickyItem(position: Int): Boolean

    /**
     * Callback allowing any customization for the view that will become sticky.
     */
    fun setupStickyItem(stickyItem: View) {}

    /**
     * Callback allowing cleanup after the previous sticky view becomes a regular view.
     */
    fun tearDownStickyItem(stickyItem: View) {}
}

/**
 * Whether the sticky item should be a header or a footer.
 */
enum class StickyItemPlacement {
    /**
     * The sticky item will be fixed at the top of the list.
     *
     * If the list is scrolled down until past the sticky item's position that view
     * will become a regular view and will be scrolled down as the others.
     *
     * If the list is scrolled up past the sticky item's position that view
     * will be anchored to the top of the list, always being shown as the first item.
     */
    TOP,

    /**
     * The sticky item will be fixed at the bottom of the list.
     *
     * If the list is scrolled up until past the sticky item's position that view
     * will become a regular view and will be scrolled up as the others.
     *
     * If the list is scrolled down past the sticky item's position that view
     * will be anchored to the bottom of the list, always being shown as the last item.
     */
    BOTTOM,
}

/**
 * Vertical LinearLayoutManager that will prevent certain items from being scrolled off-screen.
 *
 * @param context [Context] needed for various Android interactions.
 * @param stickyItemPlacement whether the sticky item should be blocked from being scrolled off
 * to the top of the screen or off to the bottom of the screen.
 * @param reverseLayout When set to true, layouts from end to start.
 */
@Suppress("TooManyFunctions")
abstract class StickyItemsLinearLayoutManager<T> constructor(
    context: Context,
    private val stickyItemPlacement: StickyItemPlacement,
    reverseLayout: Boolean = false,
) : LinearLayoutManager(context, RecyclerView.VERTICAL, reverseLayout)
    where T : RecyclerView.Adapter<*>, T : StickyItemsAdapter {

    @VisibleForTesting
    internal var listAdapter: T? = null

    @VisibleForTesting(otherwise = VisibleForTesting.PROTECTED)
    internal var stickyItemPosition = RecyclerView.NO_POSITION

    @VisibleForTesting(otherwise = VisibleForTesting.PROTECTED)
    internal var stickyItemView: View? = null

    // Allows to re-evaluate and display a possibly new sticky item if data / adapter changed.
    @VisibleForTesting
    internal var stickyItemPositionsObserver = ItemPositionsAdapterDataObserver()

    // Save / Restore scroll state
    @VisibleForTesting
    internal var scrollPosition = RecyclerView.NO_POSITION

    @VisibleForTesting
    internal var scrollOffset = 0

    /**
     * @see [LinearLayoutManager.scrollToPositionWithOffset]
     *
     * @param position list item index which needs to be shown.
     * @param offset optional distance offset from the top of the list to be applied after scrolling to [position]
     * @param actuallyScrollToPositionWithOffset callback to be used for actually scrolling to an updated position
     * ad offset based on the relation with the sticky item.
     *
     * Use [setScrollState] before and after
     */
    abstract fun scrollToIndicatedPositionWithOffset(
        position: Int,
        offset: Int,
        actuallyScrollToPositionWithOffset: (Int, Int) -> Unit,
    )

    /**
     * Whether the sticky item should be shown.
     *
     * Expected to return if the sticky header item is scrolled past the list top or the sticky bottom item
     * is scrolled past the list bottom.
     */
    abstract fun shouldStickyItemBeShownForCurrentPosition(): Boolean

    /**
     * Returns the position in the Y axis to position the header appropriately,
     * depending on direction and [android.R.attr.clipToPadding].
     */
    abstract fun getY(itemView: View): Float

    override fun onAttachedToWindow(recyclerView: RecyclerView) {
        super.onAttachedToWindow(recyclerView)
        setAdapter(recyclerView.adapter)
    }

    override fun onAdapterChanged(
        oldAdapter: RecyclerView.Adapter<*>?,
        newAdapter: RecyclerView.Adapter<*>?,
    ) {
        super.onAdapterChanged(oldAdapter, newAdapter)
        setAdapter(newAdapter)
    }

    override fun onSaveInstanceState(): Parcelable {
        return SavedState(
            superState = super.onSaveInstanceState(),
            scrollPosition = scrollPosition,
            scrollOffset = scrollOffset,
        )
    }

    override fun onRestoreInstanceState(state: Parcelable?) {
        (state as? mozilla.components.browser.menu.view.SavedState)?.let {
            scrollPosition = it.scrollPosition
            scrollOffset = it.scrollOffset
            super.onRestoreInstanceState(it.superState)
        }
    }

    override fun onLayoutChildren(recycler: RecyclerView.Recycler, state: RecyclerView.State) {
        restoreView { super.onLayoutChildren(recycler, state) }

        if (!state.isPreLayout) {
            updateStickyItem(recycler, true)
        }
    }

    override fun scrollVerticallyBy(
        dy: Int,
        recycler: RecyclerView.Recycler,
        state: RecyclerView.State?,
    ): Int {
        val distanceScrolled = restoreView { super.scrollVerticallyBy(dy, recycler, state) }
        if (distanceScrolled != 0) {
            updateStickyItem(recycler, false)
        }
        return distanceScrolled
    }

    override fun findLastVisibleItemPosition(): Int =
        restoreView { super.findLastVisibleItemPosition() }

    override fun findFirstVisibleItemPosition(): Int =
        restoreView { super.findFirstVisibleItemPosition() }

    override fun findFirstCompletelyVisibleItemPosition(): Int =
        restoreView { super.findFirstCompletelyVisibleItemPosition() }

    override fun findLastCompletelyVisibleItemPosition(): Int =
        restoreView { super.findLastCompletelyVisibleItemPosition() }

    override fun computeVerticalScrollExtent(state: RecyclerView.State): Int =
        restoreView { super.computeVerticalScrollExtent(state) }

    override fun computeVerticalScrollOffset(state: RecyclerView.State): Int =
        restoreView { super.computeVerticalScrollOffset(state) }

    override fun computeVerticalScrollRange(state: RecyclerView.State): Int =
        restoreView { super.computeVerticalScrollRange(state) }

    override fun computeScrollVectorForPosition(targetPosition: Int): PointF? =
        restoreView { super.computeScrollVectorForPosition(targetPosition) }

    override fun scrollToPosition(position: Int) {
        if (stickyItemView != null) {
            scrollToPositionWithOffset(position, INVALID_OFFSET)
        } else {
            super.scrollToPosition(position)
        }
    }

    override fun scrollToPositionWithOffset(position: Int, offset: Int) {
        if (stickyItemView != null) {
            // Reset pending scroll.
            setScrollState(RecyclerView.NO_POSITION, INVALID_OFFSET)

            scrollToIndicatedPositionWithOffset(position, offset) { updatedPosition, updatedOffset ->
                super.scrollToPositionWithOffset(updatedPosition, updatedOffset)
            }

            // Remember this position and offset and scroll to it to trigger creating the sticky view.
            setScrollState(position, offset)
        } else {
            super.scrollToPositionWithOffset(position, offset)
        }
    }

    override fun onFocusSearchFailed(
        focused: View,
        focusDirection: Int,
        recycler: RecyclerView.Recycler,
        state: RecyclerView.State,
    ): View? = restoreView { super.onFocusSearchFailed(focused, focusDirection, recycler, state) }

    @VisibleForTesting(otherwise = VisibleForTesting.PROTECTED)
    internal fun getAdapterPositionForItemIndex(index: Int): Int {
        return (getChildAt(index)?.layoutParams as? RecyclerView.LayoutParams)
            ?.absoluteAdapterPosition ?: RecyclerView.NO_POSITION
    }

    @Suppress("UNCHECKED_CAST")
    @VisibleForTesting
    internal fun setAdapter(newAdapter: RecyclerView.Adapter<*>?) {
        listAdapter?.unregisterAdapterDataObserver(stickyItemPositionsObserver)

        (newAdapter as? T)?.let {
            listAdapter = newAdapter
            listAdapter?.registerAdapterDataObserver(stickyItemPositionsObserver)
            stickyItemPositionsObserver.onChanged()
        } ?: run {
            listAdapter = null
            stickyItemView = null
        }
    }

    /**
     * Perform any [operation] ignoring the sticky item. Accomplished by:
     * - detaching the sticky view
     * - performing the [operation]
     * - reattaching the sticky view.
     */
    @VisibleForTesting
    internal fun <T> restoreView(operation: () -> T): T {
        stickyItemView?.let(this::detachView)
        val result = operation()
        stickyItemView?.let(this::attachView)
        return result
    }

    /**
     * Updates the sticky item state (creation, binding, display).
     *
     * To be called whenever there's a layout or scroll.
     *
     * @param recycler [RecyclerView.Recycler] instance handling views recycling
     * @param layout whether this is called while layout or while scrolling.
     */
    @VisibleForTesting
    internal fun updateStickyItem(recycler: RecyclerView.Recycler, layout: Boolean) {
        if (shouldStickyItemBeShownForCurrentPosition()) {
            if (stickyItemView == null) {
                createStickyView(recycler, stickyItemPosition)
            }

            if (layout) {
                bindStickyItem(stickyItemView!!)
            }

            stickyItemView?.let {
                it.translationY = getY(it)
            }
        } else {
            stickyItemView?.let {
                recycleStickyItem(recycler)
            }
        }
    }

    /**
     * Construct and configure a [RecyclerView.ViewHolder] for [position],
     * including measure, layout, and data binding and assigns this to [stickyItemView].
     */
    @VisibleForTesting
    internal fun createStickyView(recycler: RecyclerView.Recycler, position: Int) {
        val stickyItem = recycler.getViewForPosition(position)

        listAdapter?.setupStickyItem(stickyItem)

        // Add sticky item as a child view, to be detached / reattached whenever
        // LinearLayoutManager#fill() is called, which happens on layout and scroll (see overrides).
        addView(stickyItem)
        measureAndLayout(stickyItem)

        // Hide this new sticky item from the parent LayoutManager, as it's fully managed by this LayoutManager.
        ignoreView(stickyItem)

        stickyItemView = stickyItem
    }

    /**
     * Binds a new [stickyItem].
     */
    @VisibleForTesting
    internal fun bindStickyItem(stickyItem: View) {
        measureAndLayout(stickyItem)

        // If we have a pending scroll wait until the end of layout and scroll again.
        if (scrollPosition != RecyclerView.NO_POSITION) {
            stickyItem.viewTreeObserver.addOnGlobalLayoutListener(
                object :
                    ViewTreeObserver.OnGlobalLayoutListener {
                    override fun onGlobalLayout() {
                        stickyItem.viewTreeObserver.removeOnGlobalLayoutListener(this)
                        if (scrollPosition != RecyclerView.NO_POSITION) {
                            scrollToPositionWithOffset(scrollPosition, scrollOffset)
                            setScrollState(RecyclerView.NO_POSITION, INVALID_OFFSET)
                        }
                    }
                },
            )
        }
    }

    /**
     * Measures and lays out [stickyItemView].
     */
    @VisibleForTesting
    internal fun measureAndLayout(stickyItem: View) {
        measureChildWithMargins(stickyItem, 0, 0)
        stickyItem.layout(
            paddingLeft,
            0,
            width - paddingRight,
            stickyItem.measuredHeight,
        )
    }

    /**
     * Returns a no longer needed [stickyItemView] View to the [RecyclerView]'s [RecyclerView.RecycledViewPool]
     * allowing it to be recycled and reused later after being re-binded in the Adapter.
     *
     * @param recycler [RecyclerView.Recycler] instance handling views recycling.
     */
    @VisibleForTesting
    internal fun recycleStickyItem(recycler: RecyclerView.Recycler?) {
        val stickyItem = stickyItemView ?: return
        stickyItemView = null

        stickyItem.translationY = 0f

        listAdapter?.tearDownStickyItem(stickyItem)

        // Stop ignoring sticky header so that it can be recycled.
        stopIgnoringView(stickyItem)

        removeView(stickyItem)
        recycler?.recycleView(stickyItem)
    }

    @VisibleForTesting
    internal fun setScrollState(position: Int, offset: Int) {
        scrollPosition = position
        scrollOffset = offset
    }

    /**
     * Observer for any changes in the items displayed or even when the Adapter changes.
     */
    @VisibleForTesting
    internal inner class ItemPositionsAdapterDataObserver : RecyclerView.AdapterDataObserver() {
        override fun onChanged() = handleChange()

        override fun onItemRangeInserted(positionStart: Int, itemCount: Int) = handleChange()

        override fun onItemRangeRemoved(positionStart: Int, itemCount: Int) = handleChange()

        override fun onItemRangeMoved(fromPosition: Int, toPosition: Int, itemCount: Int) = handleChange()

        @VisibleForTesting
        internal fun handleChange() {
            listAdapter?.let {
                stickyItemPosition = calculateNewStickyItemPosition(it)

                // Remove sticky header immediately. A layout will follow.
                if (stickyItemView != null) {
                    recycleStickyItem(null)
                }
            }
        }

        /**
         * Get the position of the closest to the anchor sticky item.
         *
         * @return sticky item's index in the adapter or RecyclerView.NO_POSITION is such an item doesn't exists.
         */
        @VisibleForTesting
        internal fun calculateNewStickyItemPosition(adapter: T): Int {
            var newStickyItemPosition = RecyclerView.NO_POSITION

            if (stickyItemPlacement == StickyItemPlacement.TOP) {
                for (i in (itemCount - 1) downTo 0) {
                    if (adapter.isStickyItem(i)) {
                        newStickyItemPosition = i
                    }
                }
            } else {
                for (i in 0 until itemCount) {
                    if (adapter.isStickyItem(i)) {
                        newStickyItemPosition = i
                    }
                }
            }

            return newStickyItemPosition
        }
    }

    companion object {
        /**
         * Get a new instance of a vertical [LinearLayoutManager] that can show one specific item
         * as a fixed header / footer in the list, be that reversed or not.
         *
         * @param stickyItemPlacement whether the sticky item should be anchored to the top or bottom of the list
         * @param reverseLayout when set to true, layouts from end to start.
         */
        fun <T> get(
            context: Context,
            stickyItemPlacement: StickyItemPlacement = StickyItemPlacement.TOP,
            reverseLayout: Boolean = false,
        ): StickyItemsLinearLayoutManager<T>
            where T : RecyclerView.Adapter<*>, T : StickyItemsAdapter {
            return when (stickyItemPlacement) {
                StickyItemPlacement.TOP -> StickyHeaderLinearLayoutManager(
                    context,
                    reverseLayout,
                )
                StickyItemPlacement.BOTTOM -> StickyFooterLinearLayoutManager(
                    context,
                    reverseLayout,
                )
            }
        }
    }
}

/**
 * Save / restore existing [RecyclerView] state and scrolling position and offset.
 */
@SuppressLint("ParcelCreator")
@Parcelize
@VisibleForTesting
internal data class SavedState(
    val superState: Parcelable?,
    val scrollPosition: Int,
    val scrollOffset: Int,
) : Parcelable
