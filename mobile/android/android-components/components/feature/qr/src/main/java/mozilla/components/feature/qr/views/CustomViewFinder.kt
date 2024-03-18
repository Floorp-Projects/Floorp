/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.qr.views

import android.content.Context
import android.content.res.Configuration
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.graphics.Path
import android.graphics.Rect
import android.os.Build
import android.text.Layout
import android.text.StaticLayout
import android.text.TextPaint
import android.util.AttributeSet
import android.view.View
import android.view.ViewGroup
import androidx.annotation.ColorInt
import androidx.annotation.Px
import androidx.annotation.StringRes
import androidx.annotation.VisibleForTesting
import androidx.appcompat.widget.AppCompatImageView
import androidx.core.content.ContextCompat
import androidx.core.text.HtmlCompat
import mozilla.components.support.ktx.android.util.dpToPx
import mozilla.components.support.ktx.android.util.spToPx
import kotlin.math.min
import kotlin.math.roundToInt

/**
 * A [View] that shows a ViewFinder positioned in center of the camera view and draws an Overlay
 */
@Suppress("LargeClass")
class CustomViewFinder @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
) : AppCompatImageView(context, attrs) {
    private var messageResource: Int? = null
    private val overlayPaint: Paint = Paint(Paint.ANTI_ALIAS_FLAG)
    internal val viewFinderPaint: Paint = Paint(Paint.ANTI_ALIAS_FLAG)
    private var viewFinderPath: Path = Path()
    private var viewFinderPathSaved: Boolean = false
    private var overlayPath: Path = Path()
    private var overlayPathSaved: Boolean = false

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal lateinit var viewFinderRectangle: Rect
    private var viewFinderCornersSize: Float = 0f
    private var viewFinderCornersRadius: Float = 0f
    private var viewFinderTop: Float = 0f
    private var viewFinderLeft: Float = 0f
    private var viewFinderRight: Float = 0f
    private var viewFinderBottom: Float = 0f
    private var normalizedRadius: Float = 0f

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal var scanMessageLayout: StaticLayout? = null
    private lateinit var messageTextPaint: TextPaint

    init {
        isSaveEnabled = true
        overlayPaint.style = Paint.Style.FILL
        viewFinderPaint.style = Paint.Style.STROKE
        overlayPath.fillType = Path.FillType.EVEN_ODD
        viewFinderPath.fillType = Path.FillType.EVEN_ODD

        this.layoutParams = ViewGroup.LayoutParams(
            ViewGroup.LayoutParams.MATCH_PARENT,
            ViewGroup.LayoutParams.MATCH_PARENT,
        )
        this.setOverlayColor(DEFAULT_OVERLAY_COLOR)
        this.setViewFinderColor(DEFAULT_VIEWFINDER_COLOR)
        this.setViewFinderStroke(DEFAULT_VIEWFINDER_THICKNESS_DP.dpToPx(resources.displayMetrics))
        this.setViewFinderCornerRadius(DEFAULT_VIEWFINDER_CORNERS_RADIUS_DP.dpToPx(resources.displayMetrics))
    }

    /** Calculates viewfinder rectangle and triggers calculating message layout that depends on it */
    internal fun computeViewFinderRect(width: Int, height: Int) {
        if (width > 0 && height > 0) {
            val minimumDimension = min(width.toFloat(), height.toFloat())

            val viewFinderSide = (minimumDimension * DEFAULT_VIEWFINDER_WIDTH_RATIO).roundToInt()

            val viewFinderLeftOrRight = (width - viewFinderSide) / 2
            val viewFinderTopOrBottom = (height - viewFinderSide) / 2
            viewFinderRectangle = Rect(
                viewFinderLeftOrRight,
                viewFinderTopOrBottom,
                viewFinderLeftOrRight + viewFinderSide,
                viewFinderTopOrBottom + viewFinderSide,
            )

            this.setViewFinderCornerSize(DEFAULT_VIEWFINDER_CORNER_SIZE_RATIO * viewFinderRectangle.width())

            viewFinderTop = viewFinderRectangle.top.toFloat()
            viewFinderLeft = viewFinderRectangle.left.toFloat()
            viewFinderRight = viewFinderRectangle.right.toFloat()
            viewFinderBottom = viewFinderRectangle.bottom.toFloat()
            normalizedRadius = min(viewFinderCornersRadius, (viewFinderCornersSize - 1).coerceAtLeast(0f))

            showMessage(scanMessageStringRes)
        }
    }

    override fun onLayout(changed: Boolean, left: Int, top: Int, right: Int, bottom: Int) {
        computeViewFinderRect(width, height)
    }

    // useful when you have a disappearing keyboard
    override fun onSizeChanged(w: Int, h: Int, oldw: Int, oldh: Int) {
        redraw()
    }

    override fun onConfigurationChanged(newConfig: Configuration?) {
        super.onConfigurationChanged(newConfig)
        redraw()
    }

    override fun onDraw(canvas: Canvas) {
        drawOverlay(canvas)
        drawViewFinder(canvas)
        drawMessage(canvas)
    }

    private fun redraw() {
        overlayPathSaved = false
        viewFinderPathSaved = false
        computeViewFinderRect(width, height)
        invalidate()
    }

    /** Draws the Overlay */
    private fun drawOverlay(canvas: Canvas) {
        if (!overlayPathSaved) {
            overlayPath.apply {
                reset()
                moveTo(viewFinderLeft, viewFinderTop + normalizedRadius)
                quadTo(viewFinderLeft, viewFinderTop, viewFinderLeft + normalizedRadius, viewFinderTop)
                lineTo(viewFinderRight - normalizedRadius, viewFinderTop)
                quadTo(viewFinderRight, viewFinderTop, viewFinderRight, viewFinderTop + normalizedRadius)
                lineTo(viewFinderRight, viewFinderBottom - normalizedRadius)
                quadTo(viewFinderRight, viewFinderBottom, viewFinderRight - normalizedRadius, viewFinderBottom)
                lineTo(viewFinderLeft + normalizedRadius, viewFinderBottom)
                quadTo(viewFinderLeft, viewFinderBottom, viewFinderLeft, viewFinderBottom - normalizedRadius)
                lineTo(viewFinderLeft, viewFinderTop + normalizedRadius)
                moveTo(0f, 0f)
                lineTo(width.toFloat(), 0f)
                lineTo(width.toFloat(), height.toFloat())
                lineTo(0f, height.toFloat())
                lineTo(0f, 0f)
            }
            overlayPathSaved = true
        }
        canvas.drawPath(overlayPath, overlayPaint)
    }

    /** Draws the ViewFinder */
    private fun drawViewFinder(canvas: Canvas) {
        if (!viewFinderPathSaved) {
            viewFinderPath.apply {
                reset()
                moveTo(viewFinderLeft, viewFinderTop + viewFinderCornersSize)
                lineTo(viewFinderLeft, viewFinderTop + normalizedRadius)
                quadTo(viewFinderLeft, viewFinderTop, viewFinderLeft + normalizedRadius, viewFinderTop)
                lineTo(viewFinderLeft + viewFinderCornersSize, viewFinderTop)
                moveTo(viewFinderRight - viewFinderCornersSize, viewFinderTop)
                lineTo(viewFinderRight - normalizedRadius, viewFinderTop)
                quadTo(viewFinderRight, viewFinderTop, viewFinderRight, viewFinderTop + normalizedRadius)
                lineTo(viewFinderRight, viewFinderTop + viewFinderCornersSize)
                moveTo(viewFinderRight, viewFinderBottom - viewFinderCornersSize)
                lineTo(viewFinderRight, viewFinderBottom - normalizedRadius)
                quadTo(viewFinderRight, viewFinderBottom, viewFinderRight - normalizedRadius, viewFinderBottom)
                lineTo(viewFinderRight - viewFinderCornersSize, viewFinderBottom)
                moveTo(viewFinderLeft + viewFinderCornersSize, viewFinderBottom)
                lineTo(viewFinderLeft + normalizedRadius, viewFinderBottom)
                quadTo(viewFinderLeft, viewFinderBottom, viewFinderLeft, viewFinderBottom - normalizedRadius)
                lineTo(viewFinderLeft, viewFinderBottom - viewFinderCornersSize)
            }
            viewFinderPathSaved = true
        }
        canvas.drawPath(viewFinderPath, this.viewFinderPaint)
    }

    /**
     * Creates a Static Layout used to show a message below the viewfinder
     */
    @Suppress("Deprecation")
    private fun showMessage(@StringRes scanMessageId: Int?) {
        val scanMessage = if (scanMessageId != null) {
            HtmlCompat.fromHtml(
                context.getString(scanMessageId),
                HtmlCompat.FROM_HTML_MODE_LEGACY,
            )
        } else {
            ""
        }
        messageTextPaint = TextPaint().apply {
            isAntiAlias = true
            color = ContextCompat.getColor(context, android.R.color.white)
            textSize = SCAN_MESSAGE_TEXT_SIZE_SP.spToPx(resources.displayMetrics)
        }

        scanMessageLayout =
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
                StaticLayout.Builder.obtain(
                    scanMessage,
                    0,
                    scanMessage.length,
                    messageTextPaint,
                    viewFinderRectangle.width(),
                ).setAlignment(Layout.Alignment.ALIGN_CENTER).build()
            } else {
                StaticLayout(
                    scanMessage,
                    messageTextPaint,
                    viewFinderRectangle.width(),
                    Layout.Alignment.ALIGN_CENTER,
                    1.0f,
                    0.0f,
                    true,
                )
            }

        messageResource = scanMessageId
    }

    /** Draws text below the ViewFinder.  */
    private fun drawMessage(canvas: Canvas) {
        canvas.save()
        canvas.translate(
            viewFinderRectangle.left.toFloat(),
            viewFinderRectangle.bottom.toFloat() +
                SCAN_MESSAGE_TOP_PADDING_DP.dpToPx(resources.displayMetrics),
        )
        scanMessageLayout?.draw(canvas)
        canvas.restore()
    }

    /** Sets the color for the Overlay.  */
    private fun setOverlayColor(@ColorInt color: Int) {
        overlayPaint.color = color
        if (isLaidOut) {
            invalidate()
        }
    }

    /** Sets the stroke color for the ViewFinder. */
    fun setViewFinderColor(@ColorInt color: Int) {
        viewFinderPaint.color = color
        if (isLaidOut) {
            invalidate()
        }
    }

    /** Sets the stroke width for the ViewFinder.  */
    private fun setViewFinderStroke(@Px stroke: Float) {
        viewFinderPaint.strokeWidth = stroke
        if (isLaidOut) {
            invalidate()
        }
    }

    /** Sets the corner size for the ViewFinder.  */
    private fun setViewFinderCornerSize(@Px size: Float) {
        viewFinderCornersSize = size
        if (isLaidOut) {
            invalidate()
        }
    }

    /** Sets the corner radius for the ViewFinder. */
    private fun setViewFinderCornerRadius(@Px radius: Float) {
        viewFinderCornersRadius = radius
        if (isLaidOut) {
            invalidate()
        }
    }

    companion object {
        internal const val DEFAULT_VIEWFINDER_WIDTH_RATIO = 0.5f
        private const val DEFAULT_OVERLAY_COLOR = 0x77000000
        private const val DEFAULT_VIEWFINDER_COLOR = Color.WHITE
        private const val DEFAULT_VIEWFINDER_THICKNESS_DP = 4f
        private const val DEFAULT_VIEWFINDER_CORNER_SIZE_RATIO = 0.25f
        private const val DEFAULT_VIEWFINDER_CORNERS_RADIUS_DP = 8f
        private const val SCAN_MESSAGE_TOP_PADDING_DP = 10f
        internal const val SCAN_MESSAGE_TEXT_SIZE_SP = 12f
        internal var scanMessageStringRes: Int? = null

        /** Sets the message to be displayed below ViewFinder. */
        fun setMessage(scanMessageStringRes: Int?) {
            this.scanMessageStringRes = scanMessageStringRes
        }
    }
}
