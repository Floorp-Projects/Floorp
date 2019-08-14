/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Copyright (C) 2015 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package org.mozilla.fenix.quickactionsheet

import android.animation.ValueAnimator
import android.content.Context
import android.os.Parcel
import android.os.Parcelable
import android.util.AttributeSet
import android.view.MotionEvent
import android.view.VelocityTracker
import android.view.View
import android.view.ViewConfiguration
import android.view.ViewGroup
import android.view.accessibility.AccessibilityEvent
import androidx.annotation.IntDef
import androidx.annotation.RestrictTo
import androidx.annotation.RestrictTo.Scope.LIBRARY_GROUP
import androidx.annotation.VisibleForTesting
import androidx.coordinatorlayout.widget.CoordinatorLayout
import androidx.coordinatorlayout.widget.CoordinatorLayout.LayoutParams
import androidx.core.math.MathUtils
import androidx.core.view.ViewCompat
import androidx.customview.widget.ViewDragHelper
import com.google.android.material.shape.MaterialShapeDrawable
import com.google.android.material.shape.ShapeAppearanceModel
import mozilla.components.browser.toolbar.BrowserToolbar
import org.mozilla.fenix.R
import java.lang.ref.WeakReference
import kotlin.math.abs
import kotlin.math.max

/**
 * An interaction behavior plugin for a child view of [CoordinatorLayout] to make it work as a
 * bottom sheet. This custom behavior is for non-modal bottom sheets that should not block accessibility
 * access to the rest of the screen controls.
 */
@Suppress("TooManyFunctions", "ComplexMethod", "LargeClass")
open class QuickActionSheetBehavior<V : View>(context: Context, attrs: AttributeSet) :
    CoordinatorLayout.Behavior<V>(context, attrs) {

    /**
     * Save flags to be preserved in bottomsheet on configuration change.
     *
     * @param flags bitwise int of [.SAVE_PEEK_HEIGHT], [.SAVE_FIT_TO_CONTENTS],
     * [.SAVE_HIDEABLE], [.SAVE_SKIP_COLLAPSED], [.SAVE_ALL] and
     * [.SAVE_NONE].
     * @see .getSaveFlags
     * @attr ref com.google.android.material.R.styleable#QuickActionSheetBehavior_Layout_behavior_saveFlags
     */
    var saveFlags = SAVE_NONE

    private var fitToContents = true

    private val maximumVelocity: Float

    /** Peek height set by the user.  */
    private var peekHeight: Int = 0

    /** Whether or not to use automatic peek height.  */
    private var peekHeightAuto: Boolean = false

    /** Minimum peek height permitted.  */
    @get:VisibleForTesting
    internal var peekHeightMin: Int = 0
        private set

    /** True if Behavior has a non-null value for the @shapeAppearance attribute  */
//    private val shapeThemingEnabled: Boolean

    private var materialShapeDrawable: MaterialShapeDrawable? = null

    /** Default Shape Appearance to be used in bottomsheet  */
    private var shapeAppearanceModelDefault: ShapeAppearanceModel? = null

    private var interpolatorAnimator: ValueAnimator? = null

    internal var expandedOffset: Int = 0

    internal var fitToContentsOffset: Int = 0

    internal var halfExpandedOffset: Int = 0

    internal var halfExpandedRatio = HALF_EXPANDED_RATIO_DEFAULT

    internal var collapsedOffset: Int = 0

    internal var elevation = -1f

    internal var hideable: Boolean = false

    /**
     * Sets whether this bottom sheet should skip the collapsed state when it is being hidden after it
     * is expanded once. Setting this to true has no effect unless the sheet is hideable.
     *
     * @param skipCollapsed True if the bottom sheet should skip the collapsed state.
     * @attr ref com.google.android.material.R.styleable#QuickActionSheetBehavior_Layout_behavior_skipCollapsed
     */
    var skipCollapsed: Boolean = false

    @State
    var state
        get() = internalState
        set(value) {
            @State val previousState = this.internalState
            if (value == this.internalState) {
                return
            }
            if (viewRef == null) {
                // The view is not laid out yet; modify mState and let onLayoutChild handle it later
                @Suppress("ComplexCondition")
                if ((value == STATE_COLLAPSED || value == STATE_EXPANDED ||
                            value == STATE_HALF_EXPANDED || (hideable && value == STATE_HIDDEN))
                ) {
                    this.internalState = value
                }
                return
            }
            startSettlingAnimationPendingLayout(value)
            updateDrawableOnStateChange(value, previousState)
        }

    @State
    internal var internalState = STATE_COLLAPSED

    internal var viewDragHelper: ViewDragHelper? = null

    private var ignoreEvents: Boolean = false

    private var lastNestedScrollDy: Int = 0

    private var nestedScrolled: Boolean = false

    internal var parentWidth: Int = 0
    internal var parentHeight: Int = 0

    internal var viewRef: WeakReference<V>? = null

    internal var nestedScrollingChildRef: WeakReference<View>? = null

    private var callback: QuickActionSheetCallback? = null

    private var velocityTracker: VelocityTracker? = null

    internal var activePointerId: Int = 0

    private var initialY: Int = 0

    internal var touchingScrollingChild: Boolean = false

    /**
     * Sets whether the height of the expanded sheet is determined by the height of its contents, or
     * if it is expanded in two stages (half the height of the parent container, full height of parent
     * container). Default value is true.
     *
     * @param fitToContents whether or not to fit the expanded sheet to its contents.
     */
    // If sheet is already laid out, recalculate the collapsed offset based on new setting.
    // Otherwise, let onLayoutChild handle this later.
    // Fix incorrect expanded settings depending on whether or not we are fitting sheet to contents.
    var isFitToContents: Boolean
        get() = fitToContents
        set(fitToContents) {
            if (this.fitToContents == fitToContents) {
                return
            }
            this.fitToContents = fitToContents
            if (viewRef != null) {
                calculateCollapsedOffset()
            }
            setStateInternal(
                if (this.fitToContents && internalState == STATE_HALF_EXPANDED)
                    STATE_EXPANDED else internalState
            )
        }

    /**
     * Sets whether this bottom sheet can hide when it is swiped down.
     *
     * @param hideable `true` to make this bottom sheet hideable.
     * @attr ref com.google.android.material.R.styleable#QuickActionSheetBehavior_Layout_behavior_hideable
     */
    // Lift up to collapsed state
    var isHideable: Boolean
        get() = hideable
        set(hideable) {
            if (this.hideable != hideable) {
                this.hideable = hideable
                if (!hideable && internalState == STATE_HIDDEN) {
                    state = STATE_COLLAPSED
                }
            }
        }

    private val yVelocity: Float
        get() {
            if (velocityTracker == null) {
                return 0f
            }
            velocityTracker!!.computeCurrentVelocity(PIXELS_PER_SECOND_IN_MS, maximumVelocity)
            return velocityTracker!!.getYVelocity(activePointerId)
        }

    private val dragCallback = object : ViewDragHelper.Callback() {

        @Suppress("ReturnCount")
        override fun tryCaptureView(child: View, pointerId: Int): Boolean {
            if (internalState == STATE_DRAGGING) {
                return false
            }
            if (touchingScrollingChild) {
                return false
            }
            if (internalState == STATE_EXPANDED && activePointerId == pointerId) {
                val scroll = if (nestedScrollingChildRef != null) nestedScrollingChildRef!!.get() else null
                if (scroll != null && scroll.canScrollVertically(-1)) {
                    // Let the content scroll up
                    return false
                }
            }
            return viewRef != null && viewRef!!.get() === child
        }

        override fun onViewPositionChanged(changedView: View, left: Int, top: Int, dx: Int, dy: Int) {
            dispatchOnSlide(top)
        }

        override fun onViewDragStateChanged(state: Int) {
            if (state == ViewDragHelper.STATE_DRAGGING) {
                setStateInternal(STATE_DRAGGING)
            }
        }

        @Suppress("ComplexCondition")
        override fun onViewReleased(releasedChild: View, xvel: Float, yvel: Float) {
            val top: Int
            @State val targetState: Int
            if (yvel < 0) { // Moving up
                if (fitToContents) {
                    top = fitToContentsOffset
                    targetState = STATE_EXPANDED
                } else {
                    val currentTop = releasedChild.top
                    if (currentTop > halfExpandedOffset) {
                        top = halfExpandedOffset
                        targetState = STATE_HALF_EXPANDED
                    } else {
                        top = expandedOffset
                        targetState = STATE_EXPANDED
                    }
                }
            } else if ((hideable && shouldHide(
                    releasedChild,
                    yvel
                ) && (releasedChild.top > collapsedOffset || abs(xvel) < abs(yvel)))
            ) {
                // Hide if we shouldn't collapse and the view was either released low or it was a
                // vertical swipe.
                top = parentHeight
                targetState = STATE_HIDDEN
            } else if (yvel == 0f || abs(xvel) > abs(yvel)) {
                // If the Y velocity is 0 or the swipe was mostly horizontal indicated by the X velocity
                // being greater than the Y velocity, settle to the nearest correct height.
                val currentTop = releasedChild.top
                if (fitToContents) {
                    if ((abs(currentTop - fitToContentsOffset) < abs(currentTop - collapsedOffset))) {
                        top = fitToContentsOffset
                        targetState = STATE_EXPANDED
                    } else {
                        top = collapsedOffset
                        targetState = STATE_COLLAPSED
                    }
                } else {
                    if (currentTop < halfExpandedOffset) {
                        if (currentTop < abs(currentTop - collapsedOffset)) {
                            top = expandedOffset
                            targetState = STATE_EXPANDED
                        } else {
                            top = halfExpandedOffset
                            targetState = STATE_HALF_EXPANDED
                        }
                    } else {
                        if ((abs(currentTop - halfExpandedOffset) < abs(currentTop - collapsedOffset))) {
                            top = halfExpandedOffset
                            targetState = STATE_HALF_EXPANDED
                        } else {
                            top = collapsedOffset
                            targetState = STATE_COLLAPSED
                        }
                    }
                }
            } else { // Moving Down
                if (fitToContents) {
                    top = collapsedOffset
                    targetState = STATE_COLLAPSED
                } else {
                    // Settle to the nearest correct height.
                    val currentTop = releasedChild.top
                    if ((abs(currentTop - halfExpandedOffset) < abs(currentTop - collapsedOffset))) {
                        top = halfExpandedOffset
                        targetState = STATE_HALF_EXPANDED
                    } else {
                        top = collapsedOffset
                        targetState = STATE_COLLAPSED
                    }
                }
            }
            if (viewDragHelper!!.settleCapturedViewAt(releasedChild.left, top)) {
                setStateInternal(STATE_SETTLING)
                if (targetState == STATE_EXPANDED && interpolatorAnimator != null) {
                    interpolatorAnimator!!.reverse()
                }
                ViewCompat.postOnAnimation(
                    releasedChild, SettleRunnable(releasedChild, targetState)
                )
            } else {
                if (targetState == STATE_EXPANDED && interpolatorAnimator != null) {
                    interpolatorAnimator!!.reverse()
                }
                setStateInternal(targetState)
            }
        }

        override fun clampViewPositionVertical(child: View, top: Int, dy: Int): Int {
            return MathUtils.clamp(
                top, getExpandedOffset(), if (hideable) parentHeight else collapsedOffset
            )
        }

        override fun clampViewPositionHorizontal(child: View, left: Int, dx: Int): Int {
            return child.left
        }

        override fun getViewVerticalDragRange(child: View): Int {
            return if (hideable) {
                parentHeight
            } else {
                collapsedOffset
            }
        }
    }

    /** Callback for monitoring events about bottom sheets.  */
    interface QuickActionSheetCallback {

        /**
         * Called when the bottom sheet changes its state.
         *
         * @param bottomSheet The bottom sheet view.
         * @param newState The new state. This will be one of [.STATE_DRAGGING], [     ][.STATE_SETTLING],
         * [.STATE_EXPANDED], [.STATE_COLLAPSED], [     ][.STATE_HIDDEN], or [.STATE_HALF_EXPANDED].
         */
        fun onStateChanged(bottomSheet: View, @State newState: Int)

        /**
         * Called when the bottom sheet is being dragged.
         *
         * @param bottomSheet The bottom sheet view.
         * @param slideOffset The new offset of this bottom sheet within [-1,1] range. Offset increases
         * as this bottom sheet is moving upward. From 0 to 1 the sheet is between collapsed and
         * expanded states and from -1 to 0 it is between hidden and collapsed states.
         */
        fun onSlide(bottomSheet: View, slideOffset: Float)
    }

    /** @hide
     */
    @RestrictTo(LIBRARY_GROUP)
    @IntDef(STATE_EXPANDED, STATE_COLLAPSED, STATE_DRAGGING, STATE_SETTLING, STATE_HIDDEN, STATE_HALF_EXPANDED)
    @kotlin.annotation.Retention(AnnotationRetention.SOURCE)
    annotation class State

    init {
        val a = context.obtainStyledAttributes(attrs, R.styleable.QuickActionSheetBehavior_Layout)
//        this.shapeThemingEnabled = a.hasValue(R.styleable.QuickActionSheetBehavior_Layout_shapeAppearance)
//        val hasBackgroundTint = a.hasValue(R.styleable.QuickActionSheetBehavior_Layout_backgroundTint)
//        if (hasBackgroundTint) {
//            val bottomSheetColor = MaterialResources.getColorStateList(
//                context, a, R.styleable.QuickActionSheetBehavior_Layout_backgroundTint
//            )
//            createMaterialShapeDrawable(context, attrs, hasBackgroundTint, bottomSheetColor)
//        } else {
//        createMaterialShapeDrawable(context, attrs, hasBackgroundTint)
//        }
        createShapeValueAnimator()
        this.elevation = a.getDimension(R.styleable.QuickActionSheetBehavior_Layout_android_elevation, -1f)
        val value = a.peekValue(R.styleable.QuickActionSheetBehavior_Layout_mozac_behavior_peekHeight)
        if (value != null && value.data == PEEK_HEIGHT_AUTO) {
            setPeekHeight(value.data)
        } else {
            setPeekHeight(
                a.getDimensionPixelSize(
                    R.styleable.QuickActionSheetBehavior_Layout_mozac_behavior_peekHeight, PEEK_HEIGHT_AUTO
                )
            )
        }
        isHideable = a.getBoolean(R.styleable.QuickActionSheetBehavior_Layout_mozac_behavior_hideable, false)
        isFitToContents = a.getBoolean(R.styleable.QuickActionSheetBehavior_Layout_mozac_behavior_fitToContents, true)
        skipCollapsed = a.getBoolean(R.styleable.QuickActionSheetBehavior_Layout_mozac_behavior_skipCollapsed, false)
        saveFlags = a.getInt(R.styleable.QuickActionSheetBehavior_Layout_mozac_behavior_saveFlags, SAVE_NONE)
        setHalfExpandedRatio(
            a.getFloat(
                R.styleable.QuickActionSheetBehavior_Layout_mozac_behavior_halfExpandedRatio,
                HALF_EXPANDED_RATIO_DEFAULT
            )
        )
        setExpandedOffset(a.getInt(R.styleable.QuickActionSheetBehavior_Layout_mozac_behavior_expandedOffset, 0))
        a.recycle()
        val configuration = ViewConfiguration.get(context)
        maximumVelocity = configuration.scaledMaximumFlingVelocity.toFloat()
    }

    override fun onSaveInstanceState(parent: CoordinatorLayout, child: V): Parcelable? {
        return super.onSaveInstanceState(parent, child)?.let { SavedState(it, this) }
    }

    override fun onRestoreInstanceState(parent: CoordinatorLayout, child: V, state: Parcelable) {
        val ss = state as SavedState
        super.onRestoreInstanceState(parent, child, ss.superState!!)
        // Restore Optional State values designated by saveFlags
        restoreOptionalState(ss)
        // Intermediate states are restored as collapsed state
        if (ss.state == STATE_DRAGGING || ss.state == STATE_SETTLING) {
            this.internalState = STATE_COLLAPSED
        } else {
            this.internalState = ss.state
        }
    }

    override fun onAttachedToLayoutParams(layoutParams: LayoutParams) {
        super.onAttachedToLayoutParams(layoutParams)
        // These may already be null, but just be safe, explicitly assign them. This lets us know the
        // first time we layout with this behavior by checking (viewRef == null).
        viewRef = null
        viewDragHelper = null
    }

    override fun onDetachedFromLayoutParams() {
        super.onDetachedFromLayoutParams()
        // Release references so we don't run unnecessary codepaths while not attached to a view.
        viewRef = null
        viewDragHelper = null
    }

    override fun onLayoutChild(parent: CoordinatorLayout, child: V, layoutDirection: Int): Boolean {
        if (ViewCompat.getFitsSystemWindows(parent) && !ViewCompat.getFitsSystemWindows(child)) {
            child.fitsSystemWindows = true
        }
        // Only set MaterialShapeDrawable as background if shapeTheming is enabled, otherwise will
        // default to android:background declared in styles or layout.
//        if (shapeThemingEnabled && materialShapeDrawable != null) {
//            ViewCompat.setBackground(child, materialShapeDrawable)
//        }
        // Set elevation on MaterialShapeDrawable
        if (materialShapeDrawable != null) {
            // Use elevation attr if set on bottomsheet; otherwise, use elevation of child view.
            materialShapeDrawable!!.elevation = if (elevation == -1f) ViewCompat.getElevation(child) else elevation
        }

        if (viewRef == null) {
            // First layout with this behavior.
            peekHeightMin = parent.resources.getDimensionPixelSize(R.dimen.design_quick_action_sheet_peek_height_min)
            viewRef = WeakReference(child)
        }
        if (viewDragHelper == null) {
            viewDragHelper = ViewDragHelper.create(parent, dragCallback)
        }

        val savedTop = child.top
        // First let the parent lay it out
        parent.onLayoutChild(child, layoutDirection)
        // Offset the bottom sheet
        parentWidth = parent.width
        parentHeight = parent.height
        fitToContentsOffset = max(0, parentHeight - child.height)
        calculateHalfExpandedOffset()
        calculateCollapsedOffset()

        if (internalState == STATE_EXPANDED) {
            ViewCompat.offsetTopAndBottom(child, getExpandedOffset())
        } else if (internalState == STATE_HALF_EXPANDED) {
            ViewCompat.offsetTopAndBottom(child, halfExpandedOffset)
        } else if (hideable && internalState == STATE_HIDDEN) {
            ViewCompat.offsetTopAndBottom(child, parentHeight)
        } else if (internalState == STATE_COLLAPSED) {
            ViewCompat.offsetTopAndBottom(child, collapsedOffset)
        } else if (internalState == STATE_DRAGGING || internalState == STATE_SETTLING) {
            ViewCompat.offsetTopAndBottom(child, savedTop - child.top)
        }

        nestedScrollingChildRef = WeakReference(findScrollingChild(child)!!)
        return true
    }

    @Suppress("ReturnCount")
    override fun onInterceptTouchEvent(parent: CoordinatorLayout, child: V, event: MotionEvent): Boolean {
        if (!child.isShown) {
            ignoreEvents = true
            return false
        }
        val action = event.actionMasked
        // Record the velocity
        if (action == MotionEvent.ACTION_DOWN) {
            reset()
        }
        if (velocityTracker == null) {
            velocityTracker = VelocityTracker.obtain()
        }
        velocityTracker!!.addMovement(event)
        when (action) {
            MotionEvent.ACTION_UP, MotionEvent.ACTION_CANCEL -> {
                touchingScrollingChild = false
                activePointerId = MotionEvent.INVALID_POINTER_ID
                // Reset the ignore flag
                if (ignoreEvents) {
                    ignoreEvents = false
                    return false
                }
            }
            MotionEvent.ACTION_DOWN -> {
                val initialX = event.x.toInt()
                initialY = event.y.toInt()
                // Only intercept nested scrolling events here if the view not being moved by the
                // ViewDragHelper.
                if (internalState != STATE_SETTLING) {
                    val scroll = if (nestedScrollingChildRef != null) nestedScrollingChildRef!!.get() else null
                    if (scroll != null && parent.isPointInChildBounds(scroll, initialX, initialY)) {
                        activePointerId = event.getPointerId(event.actionIndex)
                        touchingScrollingChild = true
                    }
                }
                ignoreEvents = (activePointerId == MotionEvent.INVALID_POINTER_ID && !parent.isPointInChildBounds(
                    child,
                    initialX,
                    initialY
                ))
            }
        } // fall out
        if ((!ignoreEvents && viewDragHelper != null && viewDragHelper!!.shouldInterceptTouchEvent(event))
        ) {
            return true
        }
        // We have to handle cases that the ViewDragHelper does not capture the bottom sheet because
        // it is not the top most view of its parent. This is not necessary when the touch event is
        // happening over the scrolling content as nested scrolling logic handles that case.
        val scroll = if (nestedScrollingChildRef != null) nestedScrollingChildRef!!.get() else null
        return (action == MotionEvent.ACTION_MOVE && scroll != null &&
                !ignoreEvents && internalState != STATE_DRAGGING && !parent.isPointInChildBounds(
            scroll,
            event.x.toInt(),
            event.y.toInt()
        ) && viewDragHelper != null && abs(initialY - event.y) > viewDragHelper!!.touchSlop)
    }

    @Suppress("CollapsibleIfStatements")
    override fun onTouchEvent(parent: CoordinatorLayout, child: V, event: MotionEvent): Boolean {
        val action = event.actionMasked
        when {
            !child.isShown -> return false
            internalState == STATE_DRAGGING && action == MotionEvent.ACTION_DOWN -> return true
        }
        viewDragHelper?.processTouchEvent(event)

        // Record the velocity
        if (action == MotionEvent.ACTION_DOWN) {
            reset()
        }
        if (velocityTracker == null) {
            velocityTracker = VelocityTracker.obtain()
        }
        velocityTracker!!.addMovement(event)
        // The ViewDragHelper tries to capture only the top-most View. We have to explicitly tell it
        // to capture the bottom sheet in case it is not captured and the touch slop is passed.
        if (action == MotionEvent.ACTION_MOVE && !ignoreEvents) {
            if (abs(initialY - event.y) > viewDragHelper!!.touchSlop) {
                viewDragHelper!!.captureChildView(child, event.getPointerId(event.actionIndex))
            }
        }
        return !ignoreEvents
    }

    override fun onStartNestedScroll(
        coordinatorLayout: CoordinatorLayout,
        child: V,
        directTargetChild: View,
        target: View,
        axes: Int,
        type: Int
    ): Boolean {
        lastNestedScrollDy = 0
        nestedScrolled = false
        return (axes and ViewCompat.SCROLL_AXIS_VERTICAL) != 0
    }

    override fun onNestedPreScroll(
        coordinatorLayout: CoordinatorLayout,
        child: V,
        target: View,
        dx: Int,
        dy: Int,
        consumed: IntArray,
        type: Int
    ) {
        if (type == ViewCompat.TYPE_NON_TOUCH) {
            // Ignore fling here. The ViewDragHelper handles it.
            return
        }
        val scrollingChild = if (nestedScrollingChildRef != null) nestedScrollingChildRef!!.get() else null
        if (target !== scrollingChild) {
            return
        }
        val currentTop = child.top
        val newTop = currentTop - dy
        if (dy > 0) { // Upward
            if (newTop < getExpandedOffset()) {
                consumed[1] = currentTop - getExpandedOffset()
                ViewCompat.offsetTopAndBottom(child, -consumed[1])
                setStateInternal(STATE_EXPANDED)
            } else {
                consumed[1] = dy
                ViewCompat.offsetTopAndBottom(child, -dy)
                setStateInternal(STATE_DRAGGING)
            }
        } else if (dy < 0) { // Downward
            if (!target.canScrollVertically(-1)) {
                if (newTop <= collapsedOffset || hideable) {
                    consumed[1] = dy
                    ViewCompat.offsetTopAndBottom(child, -dy)
                    setStateInternal(STATE_DRAGGING)
                } else {
                    consumed[1] = currentTop - collapsedOffset
                    ViewCompat.offsetTopAndBottom(child, -consumed[1])
                    setStateInternal(STATE_COLLAPSED)
                }
            }
        }
        dispatchOnSlide(child.top)
        lastNestedScrollDy = dy
        nestedScrolled = true
    }

    override fun onStopNestedScroll(
        coordinatorLayout: CoordinatorLayout,
        child: V,
        target: View,
        type: Int
    ) {
        if (child.top == getExpandedOffset()) {
            setStateInternal(STATE_EXPANDED)
            return
        }
        if ((nestedScrollingChildRef == null || target !== nestedScrollingChildRef!!.get() || !nestedScrolled)
        ) {
            return
        }
        val top: Int
        val targetState: Int
        if (lastNestedScrollDy > 0) {
            top = getExpandedOffset()
            targetState = STATE_EXPANDED
        } else if (hideable && shouldHide(child, yVelocity)) {
            top = parentHeight
            targetState = STATE_HIDDEN
        } else if (lastNestedScrollDy == 0) {
            val currentTop = child.top
            if (fitToContents) {
                if (abs(currentTop - fitToContentsOffset) < abs(currentTop - collapsedOffset)) {
                    top = fitToContentsOffset
                    targetState = STATE_EXPANDED
                } else {
                    top = collapsedOffset
                    targetState = STATE_COLLAPSED
                }
            } else {
                if (currentTop < halfExpandedOffset) {
                    if (currentTop < abs(currentTop - collapsedOffset)) {
                        top = expandedOffset
                        targetState = STATE_EXPANDED
                    } else {
                        top = halfExpandedOffset
                        targetState = STATE_HALF_EXPANDED
                    }
                } else {
                    if (abs(currentTop - halfExpandedOffset) < abs(currentTop - collapsedOffset)) {
                        top = halfExpandedOffset
                        targetState = STATE_HALF_EXPANDED
                    } else {
                        top = collapsedOffset
                        targetState = STATE_COLLAPSED
                    }
                }
            }
        } else {
            if (fitToContents) {
                top = collapsedOffset
                targetState = STATE_COLLAPSED
            } else {
                // Settle to nearest height.
                val currentTop = child.top
                if (abs(currentTop - halfExpandedOffset) < abs(currentTop - collapsedOffset)) {
                    top = halfExpandedOffset
                    targetState = STATE_HALF_EXPANDED
                } else {
                    top = collapsedOffset
                    targetState = STATE_COLLAPSED
                }
            }
        }
        if (viewDragHelper!!.smoothSlideViewTo(child, child.left, top)) {
            setStateInternal(STATE_SETTLING)
            ViewCompat.postOnAnimation(child, SettleRunnable(child, targetState))
        } else {
            setStateInternal(targetState)
        }
        nestedScrolled = false
    }

    override fun onNestedScroll(
        coordinatorLayout: CoordinatorLayout,
        child: V,
        target: View,
        dxConsumed: Int,
        dyConsumed: Int,
        dxUnconsumed: Int,
        dyUnconsumed: Int,
        type: Int,
        consumed: IntArray
    ) {
        // Overridden to prevent the default consumption of the entire scroll distance.
    }

    override fun onNestedPreFling(
        coordinatorLayout: CoordinatorLayout,
        child: V,
        target: View,
        velocityX: Float,
        velocityY: Float
    ): Boolean {
        return if (nestedScrollingChildRef != null) {
            (target === nestedScrollingChildRef!!.get() && ((internalState != STATE_EXPANDED || super.onNestedPreFling(
                coordinatorLayout,
                child,
                target,
                velocityX,
                velocityY
            ))))
        } else {
            false
        }
    }

    /**
     * Sets the height of the bottom sheet when it is collapsed while optionally animating between the
     * old height and the new height.
     *
     * @param peekHeight The height of the collapsed bottom sheet in pixels,
     * or [     ][.PEEK_HEIGHT_AUTO] to configure the sheet to peek automatically at 16:9 ratio keyline.
     * @param animate Whether to animate between the old height and the new height.
     * @attr ref com.google.android.material.R.styleable#QuickActionSheetBehavior_Layout_behavior_peekHeight
     */
    @Suppress("NestedBlockDepth")
    @JvmOverloads
    fun setPeekHeight(peekHeight: Int, animate: Boolean = false) {
        var layout = false
        if (peekHeight == PEEK_HEIGHT_AUTO) {
            if (!peekHeightAuto) {
                peekHeightAuto = true
                layout = true
            }
        } else if (peekHeightAuto || this.peekHeight != peekHeight) {
            peekHeightAuto = false
            this.peekHeight = max(0, peekHeight)
            layout = true
        }
        // If sheet is already laid out, recalculate the collapsed offset based on new setting.
        // Otherwise, let onLayoutChild handle this later.
        if (layout && viewRef != null) {
            calculateCollapsedOffset()
            if (internalState == STATE_COLLAPSED) {
                val view = viewRef!!.get()
                if (view != null) {
                    if (animate) {
                        startSettlingAnimationPendingLayout(internalState)
                    } else {
                        view.requestLayout()
                    }
                }
            }
        }
    }

    /**
     * Gets the height of the bottom sheet when it is collapsed.
     *
     * @return The height of the collapsed bottom sheet in pixels, or [.PEEK_HEIGHT_AUTO] if the
     * sheet is configured to peek automatically at 16:9 ratio keyline
     * @attr ref com.google.android.material.R.styleable#QuickActionSheetBehavior_Layout_behavior_peekHeight
     */
    @Suppress("unused")
    fun getPeekHeight(): Int {
        return if (peekHeightAuto) PEEK_HEIGHT_AUTO else peekHeight
    }

    /**
     * Determines the height of the QuickActionSheet in the [.STATE_HALF_EXPANDED] state. The
     * material guidelines recommended a value of 0.5, which results in the sheet filling half of the
     * parent. The height of the QuickActionSheet will be smaller as this ratio is decreased and taller as
     * it is increased. The default value is 0.5.
     *
     * @param ratio a float between 0 and 1, representing the [.STATE_HALF_EXPANDED] ratio.
     * @attr com.google.android.material.R.styleable#QuickActionSheetBehavior_Layout_behavior_halfExpandedRatio
     */
    fun setHalfExpandedRatio(ratio: Float) {

        if ((ratio <= 0) || (ratio >= 1)) {
            throw IllegalArgumentException("ratio must be a float value between 0 and 1")
        }
        this.halfExpandedRatio = ratio
    }

    /**
     * Determines the top offset of the QuickActionSheet in the [.STATE_EXPANDED] state when
     * fitsToContent is false. The default value is 0, which results in the sheet matching the
     * parent's top.
     *
     * @param offset an integer value greater than equal to 0, representing the [     ][.STATE_EXPANDED] offset.
     * Value must not exceed the offset in the half expanded state.
     * @attr com.google.android.material.R.styleable#QuickActionSheetBehavior_Layout_behavior_expandedOffset
     */
    fun setExpandedOffset(offset: Int) {
        if (offset < 0) {
            throw IllegalArgumentException("offset must be greater than or equal to 0")
        }
        this.expandedOffset = offset
    }

    /**
     * Gets the ratio for the height of the QuickActionSheet in the [.STATE_HALF_EXPANDED] state.
     *
     * @attr com.google.android.material.R.styleable#QuickActionSheetBehavior_Layout_behavior_halfExpandedRatio
     */
    @Suppress("unused")
    fun getHalfExpandedRatio(): Float {
        return halfExpandedRatio
    }

    /**
     * Sets a callback to be notified of bottom sheet events.
     *
     * @param callback The callback to notify when bottom sheet events occur.
     */
    fun setQuickActionSheetCallback(callback: QuickActionSheetCallback) {
        this.callback = callback
    }

    private fun startSettlingAnimationPendingLayout(@State state: Int) {
        val child = viewRef?.get() ?: return
        // Start the animation; wait until a pending layout if there is one.
        val parent = child.parent
        if (parent != null && parent.isLayoutRequested && ViewCompat.isAttachedToWindow(child)) {
            val finalState = state
            child.post { startSettlingAnimation(child, finalState) }
        } else {
            startSettlingAnimation(child, state)
        }
    }

    internal fun setStateInternal(@State state: Int) {
        val previousState = this.internalState

        if (this.internalState == state) {
            return
        }
        this.internalState = state

        if (viewRef == null) {
            return
        }

        val bottomSheet = viewRef!!.get() ?: return

        ViewCompat.setImportantForAccessibility(
            bottomSheet, ViewCompat.IMPORTANT_FOR_ACCESSIBILITY_YES
        )
        bottomSheet.sendAccessibilityEvent(AccessibilityEvent.TYPE_WINDOW_STATE_CHANGED)

        updateDrawableOnStateChange(state, previousState)
        if (callback != null) {
            callback!!.onStateChanged(bottomSheet, state)
        }
    }

    private fun updateDrawableOnStateChange(@State state: Int, @State previousState: Int) {
        if (materialShapeDrawable != null) {
            val isOpening =
                state == STATE_EXPANDED && (previousState == STATE_HIDDEN || previousState == STATE_COLLAPSED)
            // If the QuickActionSheetBehavior's state is set directly to STATE_EXPANDED from
            // STATE_HIDDEN or STATE_COLLAPSED, bypassing  STATE_DRAGGING, the corner transition animation
            // will not be triggered automatically, so we will trigger it here.
            if ((isOpening && interpolatorAnimator != null && interpolatorAnimator!!.animatedFraction == 1f)
            ) {
                interpolatorAnimator!!.reverse()
            }
            if ((state == STATE_DRAGGING && previousState == STATE_EXPANDED && interpolatorAnimator != null)
            ) {
                interpolatorAnimator!!.start()
            }
        }
    }

    private fun calculateCollapsedOffset() {
        val peek: Int = if (peekHeightAuto) {
            max(peekHeightMin, parentHeight - parentWidth * AUTO_ASPECT_RATIO_SHORT / AUTO_ASPECT_RATIO_LONG)
        } else {
            peekHeight
        }

        collapsedOffset = if (fitToContents) {
            max(parentHeight - peek, fitToContentsOffset)
        } else {
            parentHeight - peek
        }
    }

    private fun calculateHalfExpandedOffset() {
        this.halfExpandedOffset = (parentHeight * (1 - halfExpandedRatio)).toInt()
    }

    private fun reset() {
        activePointerId = ViewDragHelper.INVALID_POINTER
        if (velocityTracker != null) {
            velocityTracker!!.recycle()
            velocityTracker = null
        }
    }

    private fun restoreOptionalState(ss: SavedState) {
        if (this.saveFlags == SAVE_NONE) {
            return
        }
        if (this.saveFlags == SAVE_ALL || (this.saveFlags and SAVE_PEEK_HEIGHT) == SAVE_PEEK_HEIGHT) {
            this.peekHeight = ss.peekHeight
        }
        if ((this.saveFlags == SAVE_ALL || (this.saveFlags and SAVE_FIT_TO_CONTENTS) == SAVE_FIT_TO_CONTENTS)) {
            this.fitToContents = ss.fitToContents
        }
        if (this.saveFlags == SAVE_ALL || (this.saveFlags and SAVE_HIDEABLE) == SAVE_HIDEABLE) {
            this.hideable = ss.hideable
        }
        if ((this.saveFlags == SAVE_ALL || (this.saveFlags and SAVE_SKIP_COLLAPSED) == SAVE_SKIP_COLLAPSED)) {
            this.skipCollapsed = ss.skipCollapsed
        }
    }

    internal fun shouldHide(child: View, yvel: Float): Boolean {
        if (skipCollapsed) {
            return true
        }
        if (child.top < collapsedOffset) {
            // It should not hide, but collapse.
            return false
        }
        val newTop = child.top + yvel * HIDE_FRICTION
        return abs(newTop - collapsedOffset) / peekHeight.toFloat() > HIDE_THRESHOLD
    }

    @VisibleForTesting
    internal fun findScrollingChild(view: View): View? {
        if (ViewCompat.isNestedScrollingEnabled(view)) {
            return view
        }
        if (view is ViewGroup) {
            var i = 0
            val count = view.childCount
            while (i < count) {
                val scrollingChild = findScrollingChild(view.getChildAt(i))
                if (scrollingChild != null) {
                    return scrollingChild
                }
                i++
            }
        }
        return null
    }

//    private fun createMaterialShapeDrawable(
//        context: Context, attrs: AttributeSet, hasBackgroundTint: Boolean
//    ) {
//        this.createMaterialShapeDrawable(context, attrs, hasBackgroundTint, null)
//    }

//    private fun createMaterialShapeDrawable(
//        context: Context,
//        attrs: AttributeSet,
//        hasBackgroundTint: Boolean,
//        bottomSheetColor: ColorStateList?
//    ) {
//        if (this.shapeThemingEnabled) {
//            this.shapeAppearanceModelDefault =
//                ShapeAppearanceModel(context, attrs, R.attr.bottomSheetStyle, DEF_STYLE_RES)
//
//            this.materialShapeDrawable = MaterialShapeDrawable(shapeAppearanceModelDefault)
//            this.materialShapeDrawable!!.initializeElevationOverlay(context)
//
//            if (hasBackgroundTint && bottomSheetColor != null) {
//                materialShapeDrawable!!.fillColor = bottomSheetColor
//            } else {
//                // If the tint isn't set, use the theme default background color.
//                val defaultColor = TypedValue()
//                context.theme.resolveAttribute(android.R.attr.colorBackground, defaultColor, true)
//                materialShapeDrawable!!.setTint(defaultColor.data)
//            }
//        }
//    }

    private fun createShapeValueAnimator() {
        interpolatorAnimator = ValueAnimator.ofFloat(0f, 1f)
        interpolatorAnimator!!.duration = CORNER_ANIMATION_DURATION.toLong()
        interpolatorAnimator!!.addUpdateListener { animation ->
            val value = animation.animatedValue as Float
            if (materialShapeDrawable != null) {
                materialShapeDrawable!!.interpolation = value
            }
        }
    }

    private fun getExpandedOffset(): Int {
        return if (fitToContents) fitToContentsOffset else expandedOffset
    }

    internal fun startSettlingAnimation(child: View?, state: Int) {
        var localState = state
        var top: Int
        if (localState == STATE_COLLAPSED) {
            top = collapsedOffset
        } else if (localState == STATE_HALF_EXPANDED) {
            top = halfExpandedOffset
            if (fitToContents && top <= fitToContentsOffset) {
                // Skip to the expanded state if we would scroll past the height of the contents.
                localState = STATE_EXPANDED
                top = fitToContentsOffset
            }
        } else if (localState == STATE_EXPANDED) {
            top = getExpandedOffset()
        } else if (hideable && localState == STATE_HIDDEN) {
            top = parentHeight
        } else {
            throw IllegalArgumentException("Illegal state argument: $state")
        }
        if (viewDragHelper!!.smoothSlideViewTo(child!!, child.left, top)) {
            setStateInternal(STATE_SETTLING)
            ViewCompat.postOnAnimation(child, SettleRunnable(child, localState))
        } else {
            setStateInternal(localState)
        }
    }

    internal fun dispatchOnSlide(top: Int) {
        val bottomSheet = viewRef!!.get()
        if (bottomSheet != null && callback != null) {
            if (top > collapsedOffset) {
                callback!!.onSlide(
                    bottomSheet, (collapsedOffset - top).toFloat() / (parentHeight - collapsedOffset)
                )
            } else {
                callback!!.onSlide(
                    bottomSheet, (collapsedOffset - top).toFloat() / (collapsedOffset - getExpandedOffset())
                )
            }
        }
    }

    /**
     * Disables the shaped corner [ShapeAppearanceModel] interpolation transition animations.
     * Will have no effect unless the sheet utilizes a [MaterialShapeDrawable] with set shape
     * theming properties. Only For use in UI testing.
     */
    @Suppress("unused")
    @VisibleForTesting
    fun disableShapeAnimations() {
        // Sets the shape value animator to null, prevents animations from occuring during testing.
        interpolatorAnimator = null
    }

    private inner class SettleRunnable internal constructor(
        private val view: View,
        @param:State @field:State private val targetState: Int
    ) :
        Runnable {

        override fun run() {
            if (viewDragHelper != null && viewDragHelper!!.continueSettling(true)) {
                ViewCompat.postOnAnimation(view, this)
            } else {
                if (internalState == STATE_SETTLING) {
                    setStateInternal(targetState)
                }
            }
        }
    }

    /** State persisted across instances  */
    protected class SavedState : QuickActionSavedState {
        @State
        internal var state: Int = STATE_COLLAPSED
        internal var peekHeight: Int = 0
        internal var fitToContents: Boolean = false
        internal var hideable: Boolean = false
        internal var skipCollapsed: Boolean = false

        @JvmOverloads
        constructor(source: Parcel, loader: ClassLoader? = null) : super(source, loader) {

            state = source.readInt()
            peekHeight = source.readInt()
            fitToContents = source.readInt() == 1
            hideable = source.readInt() == 1
            skipCollapsed = source.readInt() == 1
        }

        constructor(superState: Parcelable, behavior: QuickActionSheetBehavior<*>) : super(superState) {
            this.state = behavior.internalState
            this.peekHeight = behavior.peekHeight
            this.fitToContents = behavior.fitToContents
            this.hideable = behavior.hideable
            this.skipCollapsed = behavior.skipCollapsed
        }

        override fun writeToParcel(dest: Parcel, flags: Int) {
            super.writeToParcel(dest, flags)
            dest.writeInt(state)
            dest.writeInt(peekHeight)
            dest.writeInt(if (fitToContents) 1 else 0)
            dest.writeInt(if (hideable) 1 else 0)
            dest.writeInt(if (skipCollapsed) 1 else 0)
        }

        override fun describeContents(): Int {
            return 0
        }

        companion object CREATOR : Parcelable.Creator<SavedState> {
            override fun createFromParcel(parcel: Parcel): SavedState {
                return SavedState(parcel)
            }

            override fun newArray(size: Int): Array<SavedState?> {
                return newArray(size)
            }
        }
    }

    override fun layoutDependsOn(parent: CoordinatorLayout, child: V, dependency: View): Boolean {
        if (dependency is BrowserToolbar) {
            return true
        }

        return super.layoutDependsOn(parent, child, dependency)
    }

    override fun onDependentViewChanged(parent: CoordinatorLayout, child: V, dependency: View): Boolean {
        return if (dependency is BrowserToolbar) {
            repositionQuickActionSheet(child, dependency)
            true
        } else {
            false
        }
    }

    private fun repositionQuickActionSheet(quickActionSheetContainer: V, toolbar: BrowserToolbar) {
        if (toolbar.translationY >= toolbar.height.toFloat() - POSITION_SNAP_BUFFER) {
            internalState = STATE_HIDDEN
        } else if (internalState == STATE_HIDDEN || internalState == STATE_SETTLING) {
            internalState = STATE_COLLAPSED
        }
        quickActionSheetContainer.translationY = toolbar.translationY + toolbar.height * -1.0f
    }

    companion object {

        /** The bottom sheet is dragging.  */
        const val STATE_DRAGGING = 1

        /** The bottom sheet is settling.  */
        const val STATE_SETTLING = 2

        /** The bottom sheet is expanded.  */
        const val STATE_EXPANDED = 3

        /** The bottom sheet is collapsed.  */
        const val STATE_COLLAPSED = 4

        /** The bottom sheet is hidden.  */
        const val STATE_HIDDEN = 5

        /** The bottom sheet is half-expanded (used when mFitToContents is false).  */
        const val STATE_HALF_EXPANDED = 6

        /**
         * Peek at the 16:9 ratio keyline of its parent.
         *
         *
         * This can be used as a parameter for [.setPeekHeight]. [.getPeekHeight]
         * will return this when the value is set.
         */
        const val PEEK_HEIGHT_AUTO = -1

        /**
         * This flag will preserve the peekHeight int value on configuration change.
         */
        const val SAVE_PEEK_HEIGHT = 0x1

        /**
         * This flag will preserve the fitToContents boolean value on configuration change.
         */
        const val SAVE_FIT_TO_CONTENTS = 0x2

        /**
         * This flag will preserve the hideable boolean value on configuration change.
         */
        const val SAVE_HIDEABLE = 0x4

        /**
         * This flag will preserve the skipCollapsed boolean value on configuration change.
         */
        const val SAVE_SKIP_COLLAPSED = 0x8

        /**
         * This flag will preserve all aforementioned values on configuration change.
         */
        const val SAVE_ALL = -1

        /**
         * This flag will not preserve the aforementioned values set at runtime if the view is
         * destroyed and recreated. The only value preserved will be the positional state,
         * e.g. collapsed, hidden, expanded, etc. This is the default behavior.
         */
        const val SAVE_NONE = 0

        private const val HIDE_THRESHOLD = 0.5f

        private const val HIDE_FRICTION = 0.1f

        private const val CORNER_ANIMATION_DURATION = 500

        private const val PIXELS_PER_SECOND_IN_MS = 1000

        private const val HALF_EXPANDED_RATIO_DEFAULT = 0.5f

        private const val AUTO_ASPECT_RATIO_SHORT = 9

        private const val AUTO_ASPECT_RATIO_LONG = 16

        private const val DEF_STYLE_RES = R.style.Widget_Design_BottomSheet_Modal

        /**
         * A utility function to get the [QuickActionSheetBehavior] associated with the `view`.
         *
         * @param view The [View] with [QuickActionSheetBehavior].
         * @return The [QuickActionSheetBehavior] associated with the `view`.
         */
        fun <V : View> from(view: V): QuickActionSheetBehavior<V> {
            val params = view.layoutParams as? LayoutParams
                ?: throw IllegalArgumentException("The view is not a child of CoordinatorLayout")
            val behavior = params.behavior as? QuickActionSheetBehavior<*>
                ?: throw IllegalArgumentException("The view is not associated with QuickActionSheetBehavior")
            @Suppress("UNCHECKED_CAST")
            return behavior as QuickActionSheetBehavior<V>
        }
    }
}

/**
 * A [Parcelable] implementation that should be used by inheritance
 * hierarchies to ensure the state of all classes along the chain is saved.
 */
abstract class QuickActionSavedState : Parcelable {

    var superState: Parcelable? = null

    /**
     * Constructor called by derived classes when creating their SavedState objects
     *
     * @param superState The state of the superclass of this view
     */
    protected constructor(superState: Parcelable? = null) {
        this.superState = if (superState !== EMPTY_STATE) superState else null
    }

    /**
     * Constructor used when reading from a parcel. Reads the state of the superclass.
     *
     * @param source parcel to read from
     * @param loader ClassLoader to use for reading
     */
    @JvmOverloads
    protected constructor(source: Parcel, loader: ClassLoader? = null) {
        val superState = source.readParcelable<Parcelable>(loader)
        this.superState = superState ?: EMPTY_STATE
    }

    override fun describeContents(): Int {
        return 0
    }

    override fun writeToParcel(dest: Parcel, flags: Int) {
        dest.writeParcelable(superState, flags)
    }

    companion object {
        val EMPTY_STATE: QuickActionSavedState = object : QuickActionSavedState() {}

        @Suppress("unused")
        val CREATOR: Parcelable.Creator<QuickActionSavedState> =
            object : Parcelable.ClassLoaderCreator<QuickActionSavedState> {
                override fun createFromParcel(`in`: Parcel, loader: ClassLoader?): QuickActionSavedState {
                    val superState = `in`.readParcelable<Parcelable>(loader)
                    if (superState != null) {
                        throw IllegalStateException("superState must be null")
                    }
                    return EMPTY_STATE
                }

                override fun createFromParcel(`in`: Parcel): QuickActionSavedState {
                    return createFromParcel(`in`, null)
                }

                override fun newArray(size: Int): Array<QuickActionSavedState> {
                    return newArray(size)
                }
            }
    }
}
