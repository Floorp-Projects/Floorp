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
import android.view.accessibility.AccessibilityManager
import androidx.interpolator.view.animation.FastOutSlowInInterpolator
import org.mozilla.fenix.ext.increaseTapArea
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

        handle.increaseTapArea(grabHandleIncreasedTapArea)
        handle.setOnClickListener {
            bounceSheet(quickActionSheetBehavior)
        }

        val settings = Settings.getInstance(context)
        if (settings.shouldAutoBounceQuickActionSheet) {
            settings.incrementAutomaticBounceQuickActionSheetCount()
            bounceSheet(quickActionSheetBehavior, demoBounceAnimationLength)
        }
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

    companion object {
        const val grabHandleIncreasedTapArea = 50
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
        val accessibilityManager = quickActionSheetContainer.context
            .getSystemService(Context.ACCESSIBILITY_SERVICE) as AccessibilityManager

        state = when (accessibilityManager.isTouchExplorationEnabled) {
            true -> BottomSheetBehavior.STATE_EXPANDED
            false -> BottomSheetBehavior.STATE_COLLAPSED
        }

        quickActionSheetContainer.translationY = (toolbar.translationY + toolbar.height * -1.0).toFloat()
    }
}
