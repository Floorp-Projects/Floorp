/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.menu.view

import android.animation.ValueAnimator
import android.content.Context
import android.graphics.Rect
import android.view.MotionEvent
import android.view.ViewConfiguration
import android.view.ViewGroup
import android.view.animation.AccelerateDecelerateInterpolator
import android.widget.FrameLayout
import androidx.annotation.VisibleForTesting
import androidx.core.animation.doOnEnd
import androidx.core.view.children
import androidx.core.view.marginBottom
import androidx.core.view.marginLeft
import androidx.core.view.marginRight
import androidx.core.view.marginTop
import androidx.core.view.updateLayoutParams
import androidx.recyclerview.widget.RecyclerView

/**
 * ViewGroup intended to wrap another to then allow for the following automatic behavior:
 * - when first laid out on the screen the wrapped view is collapsed.
 * - informs about touches in the empty space left by the collapsed view through [blankTouchListener].
 * - when users swipe up it will expand. Once expanded it remains so.
 */
@Suppress("TooManyFunctions", "LargeClass")
internal class ExpandableLayout private constructor(context: Context) : FrameLayout(context) {
    /**
     * The wrapped view that needs to be collapsed / expanded.
     */
    @VisibleForTesting
    internal lateinit var wrappedView: ViewGroup

    /**
     * Listener of touches in the empty space left by the collapsed view.
     */
    @VisibleForTesting
    internal var blankTouchListener: (() -> Unit)? = null

    /**
     * Index of the last menu item that should be visible when the wrapped view is collapsed.
     */
    @VisibleForTesting
    internal var lastVisibleItemIndexWhenCollapsed: Int = Int.MAX_VALUE

    /**
     * Index of the sticky footer, if such an item is set.
     */
    @VisibleForTesting
    internal var stickyItemIndex: Int = RecyclerView.NO_POSITION

    /**
     * Height of wrapped view when collapsed.
     * Calculated once based on the position of the "isCollapsingMenuLimit" BrowserMenuItem.
     * Capped by [parentHeight]
     */
    @VisibleForTesting
    internal var collapsedHeight: Int = NOT_CALCULATED_DEFAULT_HEIGHT

    /**
     * Height of wrapped view when expanded.
     * Calculated once based on measuredHeighWithMargins().
     * Capped by [parentHeight]
     */
    @VisibleForTesting
    internal var expandedHeight: Int = NOT_CALCULATED_DEFAULT_HEIGHT

    /**
     * Available space given by the parent.
     */
    @VisibleForTesting
    internal var parentHeight: Int = NOT_CALCULATED_DEFAULT_HEIGHT

    /**
     * Whether to intercept touches while the view is collapsed.
     * If true:
     * - a swipe up will be intercepted and used to expand the wrapped view.
     * - a swipe in the empty space left by the collapsed view will be intercepted
     * and [blankTouchListener] will be called.
     * - other touches / gestures will be left to pass through to the children.
     */
    @VisibleForTesting
    internal var isCollapsed = true

    /**
     * Whether to intercept touches while the view is expanding.
     * If true:
     * - all touches / gestures will be intercepted.
     */
    @VisibleForTesting
    internal var isExpandInProgress = false

    /**
     * Distance in pixels a touch can wander before we think the user is scrolling.
     * (If this would be bigger than that of a child the child will react to the scroll first)
     */
    @VisibleForTesting
    internal var touchSlop = ViewConfiguration.get(context).scaledTouchSlop.toFloat()

    /**
     * Y axis coordinate of the [MotionEvent.ACTION_DOWN] event.
     * Used to calculate the distance scrolled, to know when the view should be expanded.
     */
    @VisibleForTesting
    internal var initialYCoord = NOT_CALCULATED_Y_TOUCH_COORD

    override fun onMeasure(widthMeasureSpec: Int, heightMeasureSpec: Int) {
        callParentOnMeasure(widthMeasureSpec, heightMeasureSpec)

        // Avoid new separate measure calls specifically for our usecase. Piggyback on the already requested ones.
        // Calculate our needed dimensions and collapse the menu when based on them.
        if (isCollapsed && getOrCalculateCollapsedHeight() > 0 && getOrCalculateExpandedHeight(heightMeasureSpec) > 0) {
            collapse()
        }
    }

    // While this view is collapsed (not fully expanded) we want to intercept all vertical scrolls
    // that will be used as an indicator to expand the view,
    // while letting all simple touch events get handled by children's click listeners.
    //
    // Also if this view is collapsed (full height but translated) we want to treat any touch in the
    // invisible space as a dismiss event.
    @Suppress("ComplexMethod", "ReturnCount")
    override fun onInterceptTouchEvent(ev: MotionEvent?): Boolean {
        if (shouldInterceptTouches()) {
            return when (ev?.actionMasked) {
                MotionEvent.ACTION_CANCEL, MotionEvent.ACTION_UP -> {
                    false // Allow click listeners firing for children.
                }
                MotionEvent.ACTION_DOWN -> {
                    if (isExpandInProgress) {
                        return true
                    }

                    // Check if user clicked in the empty space left by this collapsed View.
                    if (!isTouchingTheWrappedView(ev)) {
                        blankTouchListener?.invoke()
                    }

                    initialYCoord = ev.y

                    false // Allow click listeners firing for children.
                }
                MotionEvent.ACTION_MOVE -> {
                    if (isScrollingUp(ev)) {
                        expand()
                        true
                    } else {
                        false
                    }
                }
                else -> {
                    // In general, we don't want to intercept touch events.
                    // They should be handled by the child view.
                    return callParentOnInterceptTouchEvent(ev)
                }
            }
        } else {
            return if (ev != null && !isTouchingTheWrappedView(ev)) {
                // If the menu is expanded but smaller than the parent height
                // and the user touches above the menu, in the empty space.
                blankTouchListener?.invoke()
                true
            } else if (isExpandInProgress) {
                // Swallow all menu touches while the menu is expanding.
                true
            } else {
                callParentOnInterceptTouchEvent(ev)
            }
        }
    }

    @VisibleForTesting
    internal fun shouldInterceptTouches() = isCollapsed && !isExpandInProgress

    @VisibleForTesting
    internal fun isTouchingTheWrappedView(ev: MotionEvent): Boolean {
        val childrenBounds = Rect()
        wrappedView.getHitRect(childrenBounds)
        return childrenBounds.contains(ev.x.toInt(), ev.y.toInt())
    }

    @VisibleForTesting
    internal fun collapse() {
        wrappedView.translationY = parentHeight.toFloat() - collapsedHeight
        wrappedView.updateLayoutParams {
            height = collapsedHeight
        }
    }

    @VisibleForTesting
    internal fun expand() {
        isCollapsed = false
        isExpandInProgress = true

        val initialTranslation = wrappedView.translationY
        val distanceToExpandedHeight = expandedHeight - collapsedHeight
        getExpandViewAnimator(distanceToExpandedHeight).apply {
            doOnEnd {
                isExpandInProgress = false
            }

            addUpdateListener {
                wrappedView.translationY = initialTranslation - it.animatedValue as Int
                wrappedView.updateLayoutParams {
                    height = collapsedHeight + it.animatedValue as Int
                }
            }
            start()
        }
    }

    @VisibleForTesting
    internal fun getExpandViewAnimator(expandDelta: Int): ValueAnimator {
        return ValueAnimator.ofInt(0, expandDelta).apply {
            this.interpolator = AccelerateDecelerateInterpolator()
            this.duration = DEFAULT_DURATION_EXPAND_ANIMATOR
        }
    }

    @VisibleForTesting
    internal fun getOrCalculateCollapsedHeight(): Int {
        // Memoize the value.
        // Method will be called multiple times. Result will always be the same.
        if (collapsedHeight < 0) {
            collapsedHeight = calculateCollapsedHeight()
        }

        return collapsedHeight
    }

    @VisibleForTesting
    internal fun getOrCalculateExpandedHeight(heightSpec: Int): Int {
        if (expandedHeight < 0) {
            // Value from a measurement done with MeasureSpec.UNSPECIFIED.
            // May need to be capped by the parent height.
            expandedHeight = wrappedView.measuredHeight
        }

        val heightSpecSize = MeasureSpec.getSize(heightSpec)
        // heightSpecSize can be 0 for a MeasureSpec.UNSPECIFIED.
        // Ignore that, wait for a heightSpec that will contain parent height.
        if (parentHeight < 0 && heightSpecSize > 0) {
            parentHeight = heightSpecSize

            // Ensure a menu with a bigger height than the parent will be correctly laid out.
            expandedHeight = minOf(expandedHeight, parentHeight)

            // Ensure the collapsedHeight we calculated is not bigger than the expanded height
            // now capped by parent height.
            // This might happen if the menu is shown in landscape and there is no space to show
            // the lastVisibleItemIndexWhenCollapsed.
            if (collapsedHeight >= expandedHeight) {
                // If there's no space to show the lastVisibleItemIndexWhenCollapsed even if the
                // wrappedView is collapsed there's no need to collapse the view.
                collapsedHeight = expandedHeight
                isExpandInProgress = false
                isCollapsed = false
            }
        }

        return expandedHeight
    }

    @Suppress("WrongCall")
    @VisibleForTesting
    // Used for testing protected super.onMeasure(..) calls will be executed.
    internal fun callParentOnMeasure(widthSpec: Int, heightSpec: Int) {
        super.onMeasure(widthSpec, heightSpec)
    }

    @Suppress("WrongCall")
    @VisibleForTesting
    // Used for testing protected super.onInterceptTouchEvent(..) calls will be executed.
    internal fun callParentOnInterceptTouchEvent(ev: MotionEvent?): Boolean {
        return super.onInterceptTouchEvent(ev)
    }

    /**
     * Whether based on the previous movements, when considering this [event]
     * it can be inferred that the user is currently scrolling up.
     */
    @VisibleForTesting
    internal fun isScrollingUp(event: MotionEvent): Boolean {
        val yDistance = initialYCoord - event.y

        return yDistance >= touchSlop
    }

    // We need a dynamic way to get the intended collapsed height of this view before it will be laid out on the screen.
    // This method assumes the following layout:
    //          ____________________________________________________
    //  this -> |              ----------------------------------- |
    //          | ViewGroup -> |                ---------------- | |
    //          |              | RecyclerView-> |     View     | | |
    //          |              |                |     View     | | |
    //          |              |                |     View     | | |
    //          |              |                |  SpecialView | | |
    //          |              |                |     View     | | |
    //          |              |                ---------------- | |
    //          |              ----------------------------------- |
    //          ----------------------------------------------------
    // for which we want to measure the distance (height) between [this#top, half of SpecialView].
    // That distance will be the collapsed height of the ViewGroup used when this will be first shown on the screen.
    // Users will be able to afterwards expand the ViewGroup to the full height.
    @VisibleForTesting
    @Suppress("ReturnCount")
    internal fun calculateCollapsedHeight(): Int {
        val listView = (wrappedView.getChildAt(0) as RecyclerView)
        // Reconcile adapter positions with listView children positions.
        // Avoid IndexOutOfBounds / NullPointer exceptions.
        val validLastVisibleItemIndexWhenCollapsed = getChildPositionForAdapterIndex(
            listView,
            lastVisibleItemIndexWhenCollapsed,
        )
        val validStickyItemIndex = getChildPositionForAdapterIndex(
            listView,
            stickyItemIndex,
        )

        // Simple sanity check
        if (validLastVisibleItemIndexWhenCollapsed >= listView.childCount ||
            validLastVisibleItemIndexWhenCollapsed <= 0
        ) {
            return measuredHeight
        }

        var result = 0
        result += wrappedView.marginTop
        result += wrappedView.marginBottom
        result += wrappedView.paddingTop
        result += wrappedView.paddingBottom
        result += listView.marginTop
        result += listView.marginBottom
        result += listView.paddingTop
        result += listView.paddingBottom

        run loop@{
            listView.children.forEachIndexed { index, view ->
                if (index < validLastVisibleItemIndexWhenCollapsed) {
                    result += view.marginTop
                    result += view.marginBottom
                    result += view.measuredHeight
                } else if (index == validLastVisibleItemIndexWhenCollapsed) {
                    result += view.marginTop

                    // Edgecase: if the same item is the sticky footer and the lastVisibleItemIndexWhenCollapsed
                    // the menu will be collapsed to this item but shown with full height.
                    if (index == validStickyItemIndex) {
                        result += view.measuredHeight
                        return@loop
                    } else {
                        result += view.measuredHeight / 2
                    }
                } else {
                    // If there is a sticky item below we need to add it's height as an offset.
                    // Otherwise the sticky item will cover the the view of lastVisibleItemIndexWhenCollapsed.
                    if (index <= validStickyItemIndex) {
                        result += listView.getChildAt(validStickyItemIndex).measuredHeight
                    }
                    return@loop
                }
            }
        }

        return result
    }

    /**
     * In a dynamic menu - one in which items or their positions may change the adapter position and
     * the RecyclerView position for the same item may differ.
     * This method helps reconcile that.
     *
     * @return the RecyclerView position for the item at the [adapterIndex] in the adapter or
     * [RecyclerView.NO_POSITION] if there is no child for the indicated adapter position.
     */
    @VisibleForTesting
    internal fun getChildPositionForAdapterIndex(listView: RecyclerView, adapterIndex: Int): Int {
        listView.children.forEachIndexed { index, view ->
            if (listView.getChildAdapterPosition(view) == adapterIndex) {
                return index
            }
        }

        return RecyclerView.NO_POSITION
    }

    internal companion object {
        @VisibleForTesting
        const val NOT_CALCULATED_DEFAULT_HEIGHT = -1

        @VisibleForTesting
        const val NOT_CALCULATED_Y_TOUCH_COORD = 0f

        /**
         * Duration of the expand animation. Same value as the one from [R.android.integer.config_shortAnimTime]
         */
        @VisibleForTesting
        const val DEFAULT_DURATION_EXPAND_ANIMATOR = 200L

        /**
         * Wraps a content view in an [ExpandableLayout].
         *
         * @param contentView the content view to wrap.
         * @return a [ExpandableLayout] that wraps the content view.
         */
        internal fun wrapContentInExpandableView(
            contentView: ViewGroup,
            lastVisibleItemIndexWhenCollapsed: Int = Int.MAX_VALUE,
            stickyFooterItemIndex: Int = RecyclerView.NO_POSITION,
            blankTouchListener: (() -> Unit)? = null,
        ): ExpandableLayout {
            val expandableView = ExpandableLayout(contentView.context)
            val params = MarginLayoutParams(LayoutParams.WRAP_CONTENT, LayoutParams.WRAP_CONTENT)
                .apply {
                    leftMargin = contentView.marginLeft
                    topMargin = contentView.marginTop
                    rightMargin = contentView.marginRight
                    bottomMargin = contentView.marginBottom
                }
            expandableView.addView(contentView, params)

            expandableView.wrappedView = contentView
            expandableView.stickyItemIndex = stickyFooterItemIndex
            expandableView.blankTouchListener = blankTouchListener
            expandableView.lastVisibleItemIndexWhenCollapsed = lastVisibleItemIndexWhenCollapsed

            return expandableView
        }
    }
}
