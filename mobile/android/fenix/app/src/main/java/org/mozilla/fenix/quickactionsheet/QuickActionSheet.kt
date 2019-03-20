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
import androidx.interpolator.view.animation.FastOutSlowInInterpolator
import org.mozilla.fenix.ext.increaseTapArea

class QuickActionSheet @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyle: Int = 0,
    defStyleRes: Int = 0
) : LinearLayout(context, attrs, defStyle, defStyleRes) {

    init {
        inflate(getContext(), R.layout.component_quickactionsheet, this)
    }

    fun afterInflate() {
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

        bounceSheet(quickActionSheetBehavior, bounceAnimationLength)
    }

    private fun bounceSheet(
        quickActionSheetBehavior: QuickActionSheetBehavior,
        duration: Long = quickBounceAnimationLength
    ) {
        val normalPeekHeight = quickActionSheetBehavior.peekHeight
        val valueAnimator = ValueAnimator.ofFloat(normalPeekHeight.toFloat(),
            normalPeekHeight * bounceAnimationPeekHeightMultiplier)

        valueAnimator.addUpdateListener {
            quickActionSheetBehavior.peekHeight = (it.animatedValue as Float).toInt()
        }

        valueAnimator.repeatMode = ValueAnimator.REVERSE
        valueAnimator.repeatCount = 1
        valueAnimator.interpolator = FastOutSlowInInterpolator()
        valueAnimator.duration = duration
        valueAnimator.start()
    }

    companion object {
        const val grabHandleIncreasedTapArea = 50
        const val bounceAnimationLength = 500L
        const val quickBounceAnimationLength = 400L
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
        state = BottomSheetBehavior.STATE_COLLAPSED
        quickActionSheetContainer.translationY = (toolbar.translationY + toolbar.height * -1.0).toFloat()
    }
}
