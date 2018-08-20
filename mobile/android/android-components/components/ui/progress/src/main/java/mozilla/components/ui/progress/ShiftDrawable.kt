/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.ui.progress

import android.animation.ValueAnimator
import android.graphics.Canvas
import android.graphics.Path
import android.graphics.Rect
import android.graphics.drawable.Drawable
import android.view.animation.Interpolator
import android.view.animation.LinearInterpolator

internal class ShiftDrawable @JvmOverloads constructor(
    drawable: Drawable,
    duration: Int = DEFAULT_DURATION,
    interpolator: Interpolator? = LinearInterpolator()
) : DrawableWrapper(drawable) {

    private val animator = ValueAnimator.ofFloat(0f, 1f)
    private val visibleRect = Rect()
    private var path: Path? = null

    init {
        animator.duration = duration.toLong()
        animator.repeatCount = ValueAnimator.INFINITE
        animator.interpolator = interpolator ?: LinearInterpolator()
        animator.addUpdateListener {
            if (isVisible) {
                invalidateSelf()
            }
        }
    }

    override fun setVisible(visible: Boolean, restart: Boolean): Boolean {
        return super.setVisible(visible, restart).also { isDifferent ->
            when {
                isDifferent && visible -> animator.start()
                isDifferent && !visible -> animator.end()
            }
        }
    }

    override fun onBoundsChange(bounds: Rect) {
        super.onBoundsChange(bounds)
        updateBounds()
    }

    override fun onLevelChange(level: Int): Boolean {
        val result = super.onLevelChange(level)
        updateBounds()
        return result
    }

    override fun draw(canvas: Canvas) {
        val d = wrappedDrawable
        val fraction = animator.animatedFraction
        val width = visibleRect.width()
        val offset = (width * fraction).toInt()
        val stack = canvas.save()

        canvas.clipPath(path!!)

        // shift from right to left.
        // draw left-half part
        canvas.save()
        canvas.translate((-offset).toFloat(), 0f)
        d.draw(canvas)
        canvas.restore()

        // draw right-half part
        canvas.save()
        canvas.translate((width - offset).toFloat(), 0f)
        d.draw(canvas)
        canvas.restore()

        canvas.restoreToCount(stack)
    }

    private fun updateBounds() {
        val b = bounds
        val width = (b.width().toFloat() * level / MAX_LEVEL).toInt()
        val radius = b.height() / 2f
        visibleRect.set(b.left, b.top, b.left + width, b.height())

        // draw round to head of progressbar. I know it looks stupid, don't blame me now.
        path = Path().apply {
            addRect(
                b.left.toFloat(),
                b.top.toFloat(),
                b.left + width - radius,
                b.height().toFloat(),
                Path.Direction.CCW)
            addCircle(b.left + width - radius, radius, radius, Path.Direction.CCW)
        }
    }

    companion object {

        // align to ScaleDrawable implementation
        private const val MAX_LEVEL = 10000

        private const val DEFAULT_DURATION = 1000
    }
}
