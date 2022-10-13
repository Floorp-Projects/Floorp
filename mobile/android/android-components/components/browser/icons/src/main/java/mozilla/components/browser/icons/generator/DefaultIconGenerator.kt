/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.icons.generator

import android.content.Context
import android.content.res.Resources
import android.graphics.Bitmap
import android.graphics.Bitmap.Config.ARGB_8888
import android.graphics.Canvas
import android.graphics.Paint
import android.graphics.RectF
import android.util.TypedValue
import androidx.annotation.ArrayRes
import androidx.annotation.ColorInt
import androidx.annotation.ColorRes
import androidx.annotation.DimenRes
import androidx.core.content.ContextCompat
import mozilla.components.browser.icons.Icon
import mozilla.components.browser.icons.IconRequest
import mozilla.components.browser.icons.R
import mozilla.components.support.ktx.kotlin.getRepresentativeCharacter
import mozilla.components.support.ktx.kotlin.getRepresentativeSnippet
import kotlin.math.abs

/**
 * [IconGenerator] implementation that will generate an icon with a background color, rounded corners and a letter
 * representing the URL.
 */
class DefaultIconGenerator(
    @DimenRes private val cornerRadiusDimen: Int? = R.dimen.mozac_browser_icons_generator_default_corner_radius,
    @ColorRes private val textColorRes: Int = R.color.mozac_browser_icons_generator_default_text_color,
    @ArrayRes private val backgroundColorsRes: Int = R.array.mozac_browser_icons_photon_palette,
) : IconGenerator {

    override fun generate(context: Context, request: IconRequest): Icon {
        val size = context.resources.getDimension(request.size.dimen)
        val sizePx = size.toInt()

        val bitmap = Bitmap.createBitmap(sizePx, sizePx, ARGB_8888)
        val canvas = Canvas(bitmap)

        val backgroundColor = request.color ?: pickColor(context.resources, request.url)

        val paint = Paint()
        paint.color = backgroundColor

        val sizeRect = RectF(0f, 0f, size, size)
        val cornerRadius = cornerRadiusDimen?.let { context.resources.getDimension(it) } ?: 0f
        canvas.drawRoundRect(sizeRect, cornerRadius, cornerRadius, paint)

        val character = request.url.getRepresentativeCharacter()

        // The text size is calculated dynamically based on the target icon size (1/8th). For an icon
        // size of 112dp we'd use a text size of 14dp (112 / 8).
        val textSize = TypedValue.applyDimension(
            TypedValue.COMPLEX_UNIT_DIP,
            size * TARGET_ICON_RATIO,
            context.resources.displayMetrics,
        )

        paint.color = ContextCompat.getColor(context, textColorRes)
        paint.textAlign = Paint.Align.CENTER
        paint.textSize = textSize
        paint.isAntiAlias = true

        canvas.drawText(
            character,
            canvas.width / 2f,
            (canvas.height / 2f) - ((paint.descent() + paint.ascent()) / 2f),
            paint,
        )

        return Icon(
            bitmap = bitmap,
            color = backgroundColor,
            source = Icon.Source.GENERATOR,
            maskable = cornerRadius == 0f,
        )
    }

    /**
     * Return a color for this [url]. Colors will be based on the host. URLs with the same host will
     * return the same color.
     */
    @ColorInt
    internal fun pickColor(resources: Resources, url: String): Int {
        val backgroundColors = resources.obtainTypedArray(backgroundColorsRes)
        val color = if (url.isEmpty()) {
            backgroundColors.getColor(0, 0)
        } else {
            val snippet = url.getRepresentativeSnippet()
            val index = abs(snippet.hashCode() % backgroundColors.length())

            backgroundColors.getColor(index, 0)
        }

        backgroundColors.recycle()
        return color
    }

    companion object {
        private const val TARGET_ICON_RATIO = 1 / 8f
    }
}
