/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.icons.generator

import android.content.Context
import android.graphics.Bitmap
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.graphics.RectF
import android.net.Uri
import android.util.TypedValue
import mozilla.components.browser.icons.Icon
import mozilla.components.browser.icons.IconRequest
import mozilla.components.support.ktx.android.net.hostWithoutCommonPrefixes
import mozilla.components.support.ktx.android.util.dpToFloat
import mozilla.components.support.ktx.android.util.dpToPx

/**
 * [IconGenerator] implementation that will generate an icon with a background color, rounded corners and a letter
 * representing the URL.
 */
class DefaultIconGenerator(
    context: Context,
    cornerRadius: Int = DEFAULT_CORNER_RADIUS,
    private val textColor: Int = Color.WHITE,
    private val backgroundColors: IntArray = DEFAULT_COLORS
) : IconGenerator {
    private val cornerRadius: Float = cornerRadius.dpToFloat(context.resources.displayMetrics)

    @Suppress("MagicNumber")
    override fun generate(context: Context, request: IconRequest): Icon {
        val size = request.size.value.dpToPx(context.resources.displayMetrics)

        val bitmap = Bitmap.createBitmap(size, size, Bitmap.Config.ARGB_8888)
        val canvas = Canvas(bitmap)

        val backgroundColor = pickColor(request.url)

        val paint = Paint()
        paint.color = backgroundColor

        canvas.drawRoundRect(
            RectF(0f, 0f, size.toFloat(), size.toFloat()), cornerRadius, cornerRadius, paint)

        paint.color = textColor

        val character = getRepresentativeCharacter(request.url)

        // The text size is calculated dynamically based on the target icon size (1/8th). For an icon
        // size of 112dp we'd use a text size of 14dp (112 / 8).
        val textSize = TypedValue.applyDimension(
            TypedValue.COMPLEX_UNIT_DIP,
            size.toFloat() / 8.0f,
            context.resources.displayMetrics
        )

        paint.textAlign = Paint.Align.CENTER
        paint.textSize = textSize
        paint.isAntiAlias = true

        canvas.drawText(
            character,
            canvas.width / 2.0f,
            (canvas.height / 2.0f) - ((paint.descent() + paint.ascent()) / 2.0f),
            paint
        )

        return Icon(bitmap = bitmap, color = backgroundColor, source = Icon.Source.GENERATOR)
    }

    /**
     * Return a color for this [url]. Colors will be based on the host. URLs with the same host will
     * return the same color.
     */
    internal fun pickColor(url: String): Int {
        if (url.isEmpty()) {
            return backgroundColors[0]
        }

        val snippet = getRepresentativeSnippet(url)
        val index = Math.abs(snippet.hashCode() % backgroundColors.size)

        return backgroundColors[index]
    }

    /**
     * Get the representative part of the URL. Usually this is the eTLD part of the host.
     *
     * For example this method will return "facebook.com" for "https://www.facebook.com/foobar".
     */
    private fun getRepresentativeSnippet(url: String): String {
        val uri = Uri.parse(url)

        val host = uri.hostWithoutCommonPrefixes
        if (!host.isNullOrEmpty()) {
            return host
        }

        val path = uri.path
        if (!path.isNullOrEmpty()) {
            return path
        }

        return url
    }

    /**
     * Get a representative character for the given URL.
     *
     * For example this method will return "f" for "https://m.facebook.com/foobar".
     */
    internal fun getRepresentativeCharacter(url: String): String {
        val snippet = getRepresentativeSnippet(url)

        snippet.forEach { character ->
            if (character.isLetterOrDigit()) {
                return character.toUpperCase().toString()
            }
        }

        return "?"
    }

    companion object {
        // Mozilla's Visual Design Colour Palette
        // http://firefoxux.github.io/StyleGuide/#/visualDesign/colours
        private val DEFAULT_COLORS =
            intArrayOf(-0x65b400, -0x54ff73, -0xb3ff64, -0xffd164, -0xff613e, -0xff62fe, -0xae5500, -0xc9c7a6)

        private const val DEFAULT_CORNER_RADIUS = 2
    }
}
