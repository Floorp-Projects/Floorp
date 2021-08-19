/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.shortcut

import android.content.Context
import android.graphics.Bitmap
import android.graphics.BitmapFactory
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.graphics.Rect
import android.net.Uri
import android.os.Build
import android.util.TypedValue
import androidx.core.content.ContextCompat
import androidx.vectordrawable.graphics.drawable.VectorDrawableCompat
import org.mozilla.focus.R
import org.mozilla.focus.utils.UrlUtils

class IconGenerator {

    companion object {
        private val TEXT_SIZE_DP = 36f
        private val DEFAULT_ICON_CHAR = '?'
        private const val SEARCH_ICON_FRAME = 0.15

        /**
         * See [generateAdaptiveLauncherIcon] for more details.
         */
        @JvmStatic
        fun generateLauncherIcon(context: Context, url: String?): Bitmap {
            val startingChar = getRepresentativeCharacter(url)
            return generateCharacterIcon(context, startingChar)
        }

        /**
         * Generate an icon with the given character. The icon will be drawn
         * on top of a generic launcher icon shape that we provide.
         */
        private fun generateCharacterIcon(context: Context, character: Char) =
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O)
                generateAdaptiveLauncherIcon(context, character)
            else
                generateLauncherIconPreOreo(context, character)

        /*
         * This method needs to be separate from generateAdaptiveLauncherIcon so that we can generate
         * the pre-Oreo icon to display in the Add To Home screen Dialog
         */
        @JvmStatic
        fun generateLauncherIconPreOreo(context: Context, character: Char): Bitmap {
            val options = BitmapFactory.Options()
            options.inMutable = true
            val shape = BitmapFactory.decodeResource(context.resources, R.drawable.ic_homescreen_shape, options)
            return drawCharacterOnBitmap(context, character, shape)
        }

        @JvmStatic
        fun generateSearchEngineIcon(context: Context): Bitmap {
            val options = BitmapFactory.Options()
            options.inMutable = true
            val shape = BitmapFactory.decodeResource(context.resources, R.drawable.ic_search_engine_shape, options)
            return drawVectorOnBitmap(context, R.drawable.mozac_ic_search, shape, SEARCH_ICON_FRAME)
        }

        private fun drawVectorOnBitmap(context: Context, vectorId: Int, bitmap: Bitmap, frame: Double): Bitmap {
            val canvas = Canvas(bitmap)
            // Select the area to draw with a frame
            val rect = Rect(
                (frame * canvas.width).toInt(),
                (frame * canvas.height).toInt(),
                ((1 - frame) * canvas.width).toInt(),
                ((1 - frame) * canvas.height).toInt()
            )
            val icon = VectorDrawableCompat.create(context.resources, vectorId, null)
            icon!!.bounds = rect
            icon.draw(canvas)
            return bitmap
        }

        /**
         * Generates a launcher icon for versions of Android that support Adaptive Icons (Oreo+):
         * https://developer.android.com/guide/practices/ui_guidelines/icon_design_adaptive.html
         */
        private fun generateAdaptiveLauncherIcon(context: Context, character: Char): Bitmap {
            val res = context.resources
            val adaptiveIconDimen = res.getDimensionPixelSize(R.dimen.adaptive_icon_drawable_dimen)

            val bitmap = Bitmap.createBitmap(adaptiveIconDimen, adaptiveIconDimen, Bitmap.Config.ARGB_8888)
            val canvas = Canvas(bitmap)

            // Adaptive Icons have two layers: a background that fills the canvas and
            // a foreground that's centered. First, we draw the background...
            canvas.drawColor(ContextCompat.getColor(context, R.color.add_to_homescreen_icon_background))

            // Then draw the foreground
            return drawCharacterOnBitmap(context, character, bitmap)
        }

        private fun drawCharacterOnBitmap(context: Context, character: Char, bitmap: Bitmap): Bitmap {
            val canvas = Canvas(bitmap)

            val paint = Paint()

            val textSize = TypedValue.applyDimension(
                TypedValue.COMPLEX_UNIT_DIP, TEXT_SIZE_DP, context.resources.displayMetrics
            )

            paint.color = Color.WHITE
            paint.textAlign = Paint.Align.CENTER
            paint.textSize = textSize
            paint.isAntiAlias = true

            canvas.drawText(
                character.toString(),
                canvas.width / 2.0f,
                canvas.height / 2.0f - (paint.descent() + paint.ascent()) / 2.0f,
                paint
            )

            return bitmap
        }

        /**
         * Get a representative character for the given URL.
         *
         * For example this method will return "f" for "http://m.facebook.com/foobar".
         */
        @JvmStatic
        fun getRepresentativeCharacter(url: String?): Char {
            val firstChar = getRepresentativeSnippet(url)?.find { it.isLetterOrDigit() }?.uppercaseChar()
            return (firstChar ?: DEFAULT_ICON_CHAR)
        }

        /**
         * Get the representative part of the URL. Usually this is the host (without common prefixes).
         *
         * @return the representative snippet or null if one could not be found.
         */
        private fun getRepresentativeSnippet(url: String?): String? {
            if (url == null || url.isEmpty()) return null

            val uri = Uri.parse(url)
            val snippet = if (!uri.host.isNullOrEmpty()) {
                uri.host // cached by Uri class.
            } else if (!uri.path.isNullOrEmpty()) { // The uri may not have a host for e.g. file:// uri
                uri.path // cached by Uri class.
            } else {
                return null
            }

            // Strip common prefixes that we do not want to use to determine the representative characters
            return UrlUtils.stripCommonSubdomains(snippet)
        }
    }
}
