/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.ui.progress

import android.animation.Animator
import android.animation.ValueAnimator
import android.content.Context
import android.graphics.Canvas
import android.graphics.Rect
import android.graphics.drawable.Drawable
import androidx.core.view.ViewCompat
import android.util.AttributeSet
import android.view.View
import android.view.animation.AnimationUtils
import android.view.animation.LinearInterpolator
import android.widget.ProgressBar

/**
 * An animated progress bar following the Photon Design System.
 */
open class AnimatedProgressBar @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = 0
) : ProgressBar(context, attrs, defStyleAttr) {
    private var primaryAnimator: ValueAnimator? = null
    private val closingAnimator: ValueAnimator? = ValueAnimator.ofFloat(0f, 1f)
    private var clipRegion = 0f
    private var expectedProgress = 0
    private var tempRect = Rect()
    private var isRtl: Boolean = false

    private val animatorListener = ValueAnimator.AnimatorUpdateListener {
        setProgressImmediately(primaryAnimator!!.animatedValue as Int)
    }

    /**
     * Sets the current progress to the specified value.
     */
    override fun setProgress(progress: Int) {
        var nextProgress = progress
        nextProgress = Math.min(nextProgress, max)
        nextProgress = Math.max(0, nextProgress)
        expectedProgress = nextProgress

        // a dirty-hack for reloading page.
        if (expectedProgress < progress && progress == max) {
            setProgressImmediately(0)
        }

        primaryAnimator?.apply {
            cancel()
            setIntValues(progress, nextProgress)
            start()
        } ?: run { setProgressImmediately(nextProgress) }

        closingAnimator?.apply {
            if (nextProgress == max) { return@apply }

            cancel()
            clipRegion = 0f
        }
    }

    override fun onDraw(canvas: Canvas) {
        if (clipRegion == 0f) { super.onDraw(canvas); return }

        canvas.getClipBounds(tempRect)
        val clipWidth = tempRect.width() * clipRegion
        val saveCount = canvas.save()

        if (isRtl) {
            canvas.clipRect(tempRect.left.toFloat(),
                    tempRect.top.toFloat(),
                    tempRect.right - clipWidth,
                    tempRect.bottom.toFloat())
        } else {
            canvas.clipRect(tempRect.left + clipWidth,
                    tempRect.top.toFloat(),
                    tempRect.right.toFloat(),
                    tempRect.bottom.toFloat())
        }
        super.onDraw(canvas)
        canvas.restoreToCount(saveCount)
    }

    /**
     * Set the visibility state of this view.
     */
    override fun setVisibility(value: Int) {
        if (value == View.GONE) {
            if (expectedProgress == max) {
                animateClosing()
            } else {
                setVisibilityImmediately(value)
            }
        } else {
            setVisibilityImmediately(value)
        }
    }

    private fun setVisibilityImmediately(value: Int) {
        super.setVisibility(value)

        progressDrawable?.also {
            // Update the visibility state of the drawable too to start/stop animating it.
            it.setVisible(value == View.VISIBLE, true)
        }
    }

    internal fun animateClosing() {
        isRtl = ViewCompat.getLayoutDirection(this) == ViewCompat.LAYOUT_DIRECTION_RTL

        closingAnimator!!.cancel()

        val handler = handler
        handler?.postDelayed({ closingAnimator.start() }, CLOSING_DELAY.toLong())
    }

    private fun setProgressImmediately(progress: Int) {
        super.setProgress(progress)
    }

    init {
        val a = context.obtainStyledAttributes(attrs, R.styleable.AnimatedProgressBar)
        val duration = a.getInteger(R.styleable.AnimatedProgressBar_shiftDuration, DEFAULT_DURATION)
        val resID = a.getResourceId(R.styleable.AnimatedProgressBar_shiftInterpolator, DEFAULT_RESOURCE_ID)
        val wrap = a.getBoolean(R.styleable.AnimatedProgressBar_wrapShiftDrawable, true)

        primaryAnimator = ValueAnimator.ofInt(progress, max).apply {
            interpolator = LinearInterpolator()
            setDuration(PROGRESS_DURATION.toLong())
            addUpdateListener(animatorListener)
        }

        closingAnimator!!.duration = CLOSING_DURATION.toLong()
        closingAnimator.interpolator = LinearInterpolator()
        closingAnimator.addUpdateListener { valueAnimator ->
            val region = valueAnimator.animatedValue as Float
            if (clipRegion != region) {
                clipRegion = region
                invalidate()
            }
        }
        closingAnimator.addListener(object : Animator.AnimatorListener {
            override fun onAnimationStart(animator: Animator) {
                clipRegion = 0f
            }

            override fun onAnimationEnd(animator: Animator) {
                setVisibilityImmediately(View.GONE)
            }

            override fun onAnimationCancel(animator: Animator) {
                clipRegion = 0f
            }

            override fun onAnimationRepeat(animator: Animator) {}
        })

        if (progressDrawable == null) {
            progressDrawable = resources.getDrawable(R.drawable.mozac_progressbar, context.theme)
        }
        progressDrawable = buildWrapDrawable(progressDrawable, wrap, duration, resID)

        if (progress == 0) {
            // If the progress is 0 then we set the view to GONE immediately to avoid animating the
            // invisible progress.
            setVisibilityImmediately(View.GONE)
        }

        a.recycle()
    }

    private fun buildWrapDrawable(original: Drawable, isWrap: Boolean, duration: Int, resID: Int): Drawable {
        return if (isWrap) {
            val interpolator = if (resID > 0) AnimationUtils.loadInterpolator(context, resID) else null
            ShiftDrawable(original, duration, interpolator)
        } else {
            original
        }
    }

    companion object {
        private const val PROGRESS_DURATION = 200
        private const val CLOSING_DELAY = 300
        private const val CLOSING_DURATION = 300

        private const val DEFAULT_DURATION = 1000
        private const val DEFAULT_RESOURCE_ID = 0
    }
}
