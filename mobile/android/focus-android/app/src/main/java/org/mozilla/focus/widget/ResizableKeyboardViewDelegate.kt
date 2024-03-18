/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.widget

import android.animation.Animator
import android.animation.AnimatorListenerAdapter
import android.animation.ValueAnimator
import android.graphics.Rect
import android.util.AttributeSet
import android.view.View
import android.view.ViewTreeObserver
import org.mozilla.focus.R

private const val ANIMATION_LENGTH_MS = 200L

/**
 * A helper class to implement a ViewGroup that resizes dynamically (by adding padding to the bottom)
 * based on whether a keyboard is visible or not.
 *
 * Implementation based on:
 * https://github.com/mikepenz/MaterialDrawer/blob/master/library/src/main/java/com/mikepenz/materialdrawer/util/KeyboardUtil.java
 *
 * A View using this delegate needs to forward the calls to onAttachedToWindow() and onDetachedFromWindow()
 * to this class.
 */
internal class ResizableKeyboardViewDelegate(
    private val delegateView: View,
    attrs: AttributeSet?,
) {
    private val rect = Rect()
    private var decorView: View? = null
    private var shouldAnimate = false
    private var isAnimating = false
    private val layoutListener = ViewTreeObserver.OnGlobalLayoutListener {
        if (isAnimating) {
            return@OnGlobalLayoutListener
        }
        val difference = calculateDifferenceBetweenHeightAndUsableArea()

        // If difference > 0, keyboard is showing.
        // If difference =< 0, keyboard is not showing or is in multiview mode.
        if (difference > 0) {
            // Keyboard showing -> Set difference has bottom padding.
            if (delegateView.paddingBottom != difference) {
                updateBottomPadding(difference)
            }
        } else {
            // Keyboard not showing -> Reset bottom padding.
            if (delegateView.paddingBottom != 0) {
                updateBottomPadding(0)
            }
        }
    }

    init {
        val styleAttributeArray = delegateView.context.theme.obtainStyledAttributes(
            attrs,
            R.styleable.ResizableKeyboardViewDelegate,
            0,
            0,
        )
        shouldAnimate = try {
            styleAttributeArray.getBoolean(R.styleable.ResizableKeyboardViewDelegate_animate, false)
        } finally {
            styleAttributeArray.recycle()
        }
    }

    fun onAttachedToWindow() {
        delegateView.viewTreeObserver.addOnGlobalLayoutListener(layoutListener)
    }

    fun onDetachedFromWindow() {
        delegateView.viewTreeObserver.removeOnGlobalLayoutListener(layoutListener)
    }

    private fun updateBottomPadding(value: Int) {
        if (shouldAnimate) {
            animateBottomPaddingTo(value)
        } else {
            delegateView.setPadding(0, 0, 0, value)
        }
    }

    private fun animateBottomPaddingTo(value: Int) {
        isAnimating = true
        val animator = ValueAnimator.ofInt(delegateView.paddingBottom, value)
        animator.addUpdateListener { animation: ValueAnimator ->
            delegateView.setPadding(
                0,
                0,
                0,
                animation.animatedValue as Int,
            )
        }
        animator.duration = ANIMATION_LENGTH_MS
        animator.addListener(
            object : AnimatorListenerAdapter() {
                override fun onAnimationEnd(animation: Animator) {
                    isAnimating = false
                }
            },
        )
        animator.start()
    }

    private fun calculateDifferenceBetweenHeightAndUsableArea(): Int {
        if (decorView == null) {
            decorView = delegateView.rootView
        }
        decorView!!.getWindowVisibleDisplayFrame(rect)
        return if (rect.height() >= rect.width()) {
            delegateView.resources.displayMetrics.heightPixels - rect.bottom
        } else {
            delegateView.resources.displayMetrics.widthPixels - rect.right
        }
    }
}
