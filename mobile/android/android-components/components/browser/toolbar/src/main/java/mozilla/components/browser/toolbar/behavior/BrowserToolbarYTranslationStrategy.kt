/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.toolbar.behavior

import android.animation.ValueAnimator
import android.view.animation.DecelerateInterpolator
import androidx.annotation.VisibleForTesting
import mozilla.components.browser.toolbar.BrowserToolbar
import kotlin.math.max
import kotlin.math.min

@VisibleForTesting
internal const val SNAP_ANIMATION_DURATION = 150L

/**
 * Helper class with methods for different behaviors for when translating a [BottomToolbar] on the Y axis.
 */
internal abstract class BrowserToolbarYTranslationStrategy {
    @VisibleForTesting
    var animator = ValueAnimator().apply {
        interpolator = DecelerateInterpolator()
        duration = SNAP_ANIMATION_DURATION
    }

    /**
     * Snap the [BrowserToolbar] to be collapsed or expanded, depending on whatever state is closer
     * over a short amount of time.
     */
    abstract fun snapWithAnimation(toolbar: BrowserToolbar)

    /**
     * Snap the [BrowserToolbar] to be collapsed or expanded, depending on whatever state is closer immediately.
     */
    abstract fun snapImmediately(toolbar: BrowserToolbar?)

    /**
     * Translate the [BrowserToolbar] to it's full visible height.
     */
    abstract fun expandWithAnimation(toolbar: BrowserToolbar)

    /**
     * Force expanding the [BrowserToolbar] depending on the [distance] value that should be translated
     * cancelling any other translation already in progress.
     */
    abstract fun forceExpandWithAnimation(toolbar: BrowserToolbar, distance: Float)

    /**
     * Translate the [BrowserToolbar] to it's full 0 visible height.
     */
    abstract fun collapseWithAnimation(toolbar: BrowserToolbar)

    /**
     * Translate [toolbar] immediately to the specified [distance] amount (positive or negative).
     */
    abstract fun translate(toolbar: BrowserToolbar, distance: Float)

    /**
     * Translate [toolbar] to the indicated [targetTranslationY] vaue over a short amount of time.
     */
    open fun animateToTranslationY(toolbar: BrowserToolbar, targetTranslationY: Float) = with(animator) {
        addUpdateListener { toolbar.translationY = it.animatedValue as Float }
        setFloatValues(toolbar.translationY, targetTranslationY)
        start()
    }

    /**
     * Cancel any translation animations currently in progress.
     */
    fun cancelInProgressTranslation() = animator.cancel()
}

/**
 * Helper class containing methods for translating a [BrowserToolbar] on the Y axis
 * between 0 and [BrowserToolbar.getHeight]
 */
internal class BottomToolbarBehaviorStrategy : BrowserToolbarYTranslationStrategy() {
    @VisibleForTesting
    internal var wasLastExpanding = false

    override fun snapWithAnimation(toolbar: BrowserToolbar) {
        if (toolbar.translationY >= (toolbar.height / 2f)) {
            collapseWithAnimation(toolbar)
        } else {
            expandWithAnimation(toolbar)
        }
    }

    override fun snapImmediately(toolbar: BrowserToolbar?) {
        if (animator.isStarted) {
            animator.end()
        } else {
            toolbar?.apply {
                translationY = if (translationY >= height / 2) {
                    height.toFloat()
                } else {
                    0f
                }
            }
        }
    }

    override fun expandWithAnimation(toolbar: BrowserToolbar) {
        animateToTranslationY(toolbar, 0f)
    }

    override fun forceExpandWithAnimation(toolbar: BrowserToolbar, distance: Float) {
        val shouldExpandToolbar = distance < 0
        val isToolbarExpanded = toolbar.translationY == 0f
        if (shouldExpandToolbar && !isToolbarExpanded && !wasLastExpanding) {
            animator.cancel()
            expandWithAnimation(toolbar)
        }
    }

    override fun collapseWithAnimation(toolbar: BrowserToolbar) {
        animateToTranslationY(toolbar, toolbar.height.toFloat())
    }

    override fun translate(toolbar: BrowserToolbar, distance: Float) {
        toolbar.translationY =
            max(0f, min(toolbar.height.toFloat(), toolbar.translationY + distance))
    }

    override fun animateToTranslationY(toolbar: BrowserToolbar, targetTranslationY: Float) {
        wasLastExpanding = targetTranslationY <= toolbar.translationY
        super.animateToTranslationY(toolbar, targetTranslationY)
    }
}

/**
 * Helper class containing methods for translating a [BrowserToolbar] on the Y axis
 * between -[BrowserToolbar.getHeight] and 0.
 */
internal class TopToolbarBehaviorStrategy : BrowserToolbarYTranslationStrategy() {
    @VisibleForTesting
    internal var wasLastExpanding = false

    override fun snapWithAnimation(toolbar: BrowserToolbar) {
        if (toolbar.translationY >= -(toolbar.height / 2f)) {
            expandWithAnimation(toolbar)
        } else {
            collapseWithAnimation(toolbar)
        }
    }

    override fun snapImmediately(toolbar: BrowserToolbar?) {
        if (animator.isStarted) {
            animator.end()
        } else {
            toolbar?.apply {
                translationY = if (translationY >= -height / 2) {
                    0f
                } else {
                    -height.toFloat()
                }
            }
        }
    }

    override fun expandWithAnimation(toolbar: BrowserToolbar) {
        animateToTranslationY(toolbar, 0f)
    }

    override fun forceExpandWithAnimation(toolbar: BrowserToolbar, distance: Float) {
        val isExpandingInProgress = animator.isStarted && wasLastExpanding
        val shouldExpandToolbar = distance < 0
        val isToolbarExpanded = toolbar.translationY == 0f
        if (shouldExpandToolbar && !isToolbarExpanded && !isExpandingInProgress) {
            animator.cancel()
            expandWithAnimation(toolbar)
        }
    }

    override fun collapseWithAnimation(toolbar: BrowserToolbar) {
        animateToTranslationY(toolbar, -toolbar.height.toFloat())
    }

    override fun translate(toolbar: BrowserToolbar, distance: Float) {
        toolbar.translationY =
            min(0f, max(-toolbar.height.toFloat(), toolbar.translationY - distance))
    }

    override fun animateToTranslationY(toolbar: BrowserToolbar, targetTranslationY: Float) {
        wasLastExpanding = targetTranslationY >= toolbar.translationY
        super.animateToTranslationY(toolbar, targetTranslationY)
    }
}
