/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.ui.widgets.behavior

import android.animation.ValueAnimator
import android.view.View
import android.view.animation.DecelerateInterpolator
import androidx.annotation.VisibleForTesting
import kotlin.math.max
import kotlin.math.min

@VisibleForTesting
internal const val SNAP_ANIMATION_DURATION = 150L

/**
 * Helper class with methods for different behaviors for when translating a [View] on the Y axis.
 */
internal abstract class ViewYTranslationStrategy {
    @VisibleForTesting
    var animator = ValueAnimator().apply {
        interpolator = DecelerateInterpolator()
        duration = SNAP_ANIMATION_DURATION
    }

    /**
     * Snap the [View] to be collapsed or expanded, depending on whatever state is closer
     * over a short amount of time.
     */
    abstract fun snapWithAnimation(view: View)

    /**
     * Snap the [View] to be collapsed or expanded, depending on whatever state is closer immediately.
     */
    abstract fun snapImmediately(view: View?)

    /**
     * Translate the [View] to it's full visible height.
     */
    abstract fun expandWithAnimation(view: View)

    /**
     * Force expanding the [View] depending on the [distance] value that should be translated
     * cancelling any other translation already in progress.
     */
    abstract fun forceExpandWithAnimation(view: View, distance: Float)

    /**
     * Translate the [View] to it's full 0 visible height.
     */
    abstract fun collapseWithAnimation(view: View)

    /**
     * Translate [view] immediately to the specified [distance] amount (positive or negative).
     */
    abstract fun translate(view: View, distance: Float)

    /**
     * Translate [view] to the indicated [targetTranslationY] vaue over a short amount of time.
     */
    open fun animateToTranslationY(view: View, targetTranslationY: Float) = with(animator) {
        addUpdateListener { view.translationY = it.animatedValue as Float }
        setFloatValues(view.translationY, targetTranslationY)
        start()
    }

    /**
     * Cancel any translation animations currently in progress.
     */
    fun cancelInProgressTranslation() = animator.cancel()
}

/**
 * Helper class containing methods for translating a [View] on the Y axis
 * between 0 and [View.getHeight]
 */
internal class BottomViewBehaviorStrategy : ViewYTranslationStrategy() {
    @VisibleForTesting
    internal var wasLastExpanding = false

    override fun snapWithAnimation(view: View) {
        if (view.translationY >= (view.height / 2f)) {
            collapseWithAnimation(view)
        } else {
            expandWithAnimation(view)
        }
    }

    override fun snapImmediately(view: View?) {
        if (animator.isStarted) {
            animator.end()
        } else {
            view?.apply {
                translationY = if (translationY >= height / 2) {
                    height.toFloat()
                } else {
                    0f
                }
            }
        }
    }

    override fun expandWithAnimation(view: View) {
        animateToTranslationY(view, 0f)
    }

    override fun forceExpandWithAnimation(view: View, distance: Float) {
        val shouldExpandToolbar = distance < 0
        val isToolbarExpanded = view.translationY == 0f
        if (shouldExpandToolbar && !isToolbarExpanded && !wasLastExpanding) {
            animator.cancel()
            expandWithAnimation(view)
        }
    }

    override fun collapseWithAnimation(view: View) {
        animateToTranslationY(view, view.height.toFloat())
    }

    override fun translate(view: View, distance: Float) {
        view.translationY =
            max(0f, min(view.height.toFloat(), view.translationY + distance))
    }

    override fun animateToTranslationY(view: View, targetTranslationY: Float) {
        wasLastExpanding = targetTranslationY <= view.translationY
        super.animateToTranslationY(view, targetTranslationY)
    }
}

/**
 * Helper class containing methods for translating a [View] on the Y axis
 * between -[View.getHeight] and 0.
 */
internal class TopViewBehaviorStrategy : ViewYTranslationStrategy() {
    @VisibleForTesting
    internal var wasLastExpanding = false

    override fun snapWithAnimation(view: View) {
        if (view.translationY >= -(view.height / 2f)) {
            expandWithAnimation(view)
        } else {
            collapseWithAnimation(view)
        }
    }

    override fun snapImmediately(view: View?) {
        if (animator.isStarted) {
            animator.end()
        } else {
            view?.apply {
                translationY = if (translationY >= -height / 2) {
                    0f
                } else {
                    -height.toFloat()
                }
            }
        }
    }

    override fun expandWithAnimation(view: View) {
        animateToTranslationY(view, 0f)
    }

    override fun forceExpandWithAnimation(view: View, distance: Float) {
        val isExpandingInProgress = animator.isStarted && wasLastExpanding
        val shouldExpandToolbar = distance < 0
        val isToolbarExpanded = view.translationY == 0f
        if (shouldExpandToolbar && !isToolbarExpanded && !isExpandingInProgress) {
            animator.cancel()
            expandWithAnimation(view)
        }
    }

    override fun collapseWithAnimation(view: View) {
        animateToTranslationY(view, -view.height.toFloat())
    }

    override fun translate(view: View, distance: Float) {
        view.translationY =
            min(0f, max(-view.height.toFloat(), view.translationY - distance))
    }

    override fun animateToTranslationY(view: View, targetTranslationY: Float) {
        wasLastExpanding = targetTranslationY >= view.translationY
        super.animateToTranslationY(view, targetTranslationY)
    }
}
