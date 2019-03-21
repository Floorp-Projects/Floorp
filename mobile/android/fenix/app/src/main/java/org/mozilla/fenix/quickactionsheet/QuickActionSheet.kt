/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.quickactionsheet

import android.content.Context
import android.util.AttributeSet
import android.view.View
import android.widget.LinearLayout
import androidx.appcompat.widget.AppCompatImageButton
import androidx.coordinatorlayout.widget.CoordinatorLayout
import androidx.core.widget.NestedScrollView
import com.google.android.material.bottomsheet.BottomSheetBehavior
import mozilla.components.browser.toolbar.BrowserToolbar
import org.mozilla.fenix.R
import android.animation.ValueAnimator
import android.os.Bundle
import android.view.accessibility.AccessibilityEvent
import android.view.accessibility.AccessibilityNodeInfo
import androidx.interpolator.view.animation.FastOutSlowInInterpolator
import org.mozilla.fenix.utils.Settings

class QuickActionSheet @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyle: Int = 0,
    defStyleRes: Int = 0
) : LinearLayout(context, attrs, defStyle, defStyleRes) {

    init {
        inflate(getContext(), R.layout.layout_quick_action_sheet, this)
    }

    override fun onAttachedToWindow() {
        super.onAttachedToWindow()
        setupHandle()
    }

    private fun setupHandle() {
        val handle = findViewById<AppCompatImageButton>(R.id.quick_action_sheet_handle)
        val linearLayout = findViewById<LinearLayout>(R.id.quick_action_sheet)
        val quickActionSheetBehavior = BottomSheetBehavior.from(linearLayout.parent as View) as QuickActionSheetBehavior

        handle.setOnClickListener {
            bounceSheet(quickActionSheetBehavior)
        }

        handle.setAccessibilityDelegate(HandleAccessibilityDelegate(quickActionSheetBehavior))

        quickActionSheetBehavior.setBottomSheetCallback(object : BottomSheetBehavior.BottomSheetCallback() {
            override fun onStateChanged(v: View, state: Int) {
                updateImportantForAccessibility(state)
            }

            override fun onSlide(p0: View, p1: Float) {
            }
        })

        updateImportantForAccessibility(quickActionSheetBehavior.state)

        val settings = Settings.getInstance(context)
        if (settings.shouldAutoBounceQuickActionSheet) {
            settings.incrementAutomaticBounceQuickActionSheetCount()
            bounceSheet(quickActionSheetBehavior, demoBounceAnimationLength)
        }
    }

    private fun updateImportantForAccessibility(state: Int) {
        findViewById<LinearLayout>(R.id.quick_action_buttons_layout).importantForAccessibility =
            if (state == BottomSheetBehavior.STATE_COLLAPSED || state == BottomSheetBehavior.STATE_HIDDEN)
                View.IMPORTANT_FOR_ACCESSIBILITY_NO_HIDE_DESCENDANTS
            else
                View.IMPORTANT_FOR_ACCESSIBILITY_AUTO
    }

    private fun bounceSheet(
        quickActionSheetBehavior: QuickActionSheetBehavior,
        duration: Long = bounceAnimationLength
    ) {
        val normalPeekHeight = quickActionSheetBehavior.peekHeight

        val peakHeightMultiplier = if (duration == demoBounceAnimationLength)
            demoBounceAnimationPeekHeightMultiplier else bounceAnimationPeekHeightMultiplier

        ValueAnimator.ofFloat(normalPeekHeight.toFloat(),
            normalPeekHeight * peakHeightMultiplier)?.let {

            it.addUpdateListener {
                quickActionSheetBehavior.peekHeight = (it.animatedValue as Float).toInt()
            }

            it.repeatMode = ValueAnimator.REVERSE
            it.repeatCount = 1
            it.interpolator = FastOutSlowInInterpolator()
            it.duration = duration
            it.start()
        }
    }

    class HandleAccessibilityDelegate(
        private val quickActionSheetBehavior: QuickActionSheetBehavior
    ) : View.AccessibilityDelegate() {
        private var finalState = BottomSheetBehavior.STATE_COLLAPSED
        get() = when (quickActionSheetBehavior.state) {
            BottomSheetBehavior.STATE_EXPANDED,
            BottomSheetBehavior.STATE_HIDDEN,
            BottomSheetBehavior.STATE_COLLAPSED -> {
                quickActionSheetBehavior.state
            }
            else -> field
        }
        set(value) {
            field = value
            quickActionSheetBehavior.state = value
        }

        override fun performAccessibilityAction(host: View?, action: Int, args: Bundle?): Boolean {
            when (action) {
                AccessibilityNodeInfo.ACTION_CLICK -> {
                    finalState = when (quickActionSheetBehavior.state) {
                        BottomSheetBehavior.STATE_EXPANDED -> BottomSheetBehavior.STATE_COLLAPSED
                        else -> BottomSheetBehavior.STATE_EXPANDED
                    }
                }
                AccessibilityNodeInfo.ACTION_COLLAPSE ->
                    finalState = BottomSheetBehavior.STATE_COLLAPSED
                AccessibilityNodeInfo.ACTION_EXPAND ->
                    finalState = BottomSheetBehavior.STATE_EXPANDED
                else -> return super.performAccessibilityAction(host, action, args)
            }

            host?.sendAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_CLICKED)

            return true
        }

        override fun onInitializeAccessibilityNodeInfo(host: View?, info: AccessibilityNodeInfo?) {
            super.onInitializeAccessibilityNodeInfo(host, info)
            info?.addAction(when (finalState) {
                BottomSheetBehavior.STATE_COLLAPSED,
                BottomSheetBehavior.STATE_HIDDEN -> AccessibilityNodeInfo.AccessibilityAction.ACTION_EXPAND
                else -> AccessibilityNodeInfo.AccessibilityAction.ACTION_COLLAPSE
            })
        }
    }

    companion object {
        const val demoBounceAnimationLength = 600L
        const val bounceAnimationLength = 400L
        const val demoBounceAnimationPeekHeightMultiplier = 4.5f
        const val bounceAnimationPeekHeightMultiplier = 3f
    }
}

@Suppress("unused") // Referenced from XML
class QuickActionSheetBehavior(
    context: Context,
    attrs: AttributeSet
) : BottomSheetBehavior<NestedScrollView>(context, attrs) {
    override fun layoutDependsOn(parent: CoordinatorLayout, child: NestedScrollView, dependency: View): Boolean {
        if (dependency is BrowserToolbar) {
            return true
        }

        return super.layoutDependsOn(parent, child, dependency)
    }

    override fun onDependentViewChanged(
        parent: CoordinatorLayout,
        child: NestedScrollView,
        dependency: View
    ): Boolean {
        return if (dependency is BrowserToolbar) {
            repositionQuickActionSheet(child, dependency)
            true
        } else {
            false
        }
    }

    private fun repositionQuickActionSheet(quickActionSheetContainer: NestedScrollView, toolbar: BrowserToolbar) {
        quickActionSheetContainer.translationY = (toolbar.translationY + toolbar.height * -1.0).toFloat()
    }
}
