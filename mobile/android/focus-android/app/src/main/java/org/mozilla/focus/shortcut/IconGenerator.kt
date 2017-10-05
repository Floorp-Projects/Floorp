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
import android.net.Uri
import android.support.annotation.VisibleForTesting
import android.util.TypedValue

import org.mozilla.focus.R
import org.mozilla.focus.utils.UrlUtils

class IconGenerator {

    companion object {
        private val ICON_SIZE = 96
        private val TEXT_SIZE_DP = 36f

        private val DEFAULT_ICON_CHAR = "?"

        /**
         * Use the given raw website icon and generate a launcher icon from it. If the given icon is null
         * or too small then an icon will be generated based on the website's URL. The icon will be drawn
         * on top of a generic launcher icon shape that we provide.
         */
        @JvmStatic
        fun generateLauncherIcon(context: Context, url: String?): Bitmap {
            val options = BitmapFactory.Options()
            options.inMutable = true

            val shape = BitmapFactory.decodeResource(context.resources, R.drawable.ic_homescreen_shape, options)

            val icon = generateIcon(context, url)

            val drawX = shape.width / 2f - icon.width / 2f
            val drawY = shape.height / 2f - icon.height / 2f

            val canvas = Canvas(shape)
            canvas.drawBitmap(icon, drawX, drawY, Paint())

            return shape
        }

        /**
         * Generate an icon for this website based on the URL.
         */
        private fun generateIcon(context: Context, url: String?): Bitmap {
            val bitmap = Bitmap.createBitmap(ICON_SIZE, ICON_SIZE, Bitmap.Config.ARGB_8888)
            val canvas = Canvas(bitmap)

            val paint = Paint()

            val character = getRepresentativeCharacter(url)

            val textSize = TypedValue.applyDimension(
                    TypedValue.COMPLEX_UNIT_DIP, TEXT_SIZE_DP, context.resources.displayMetrics)

            paint.color = Color.WHITE
            paint.textAlign = Paint.Align.CENTER
            paint.textSize = textSize
            paint.isAntiAlias = true

            canvas.drawText(character,
                    canvas.width / 2.0f,
                    canvas.height / 2.0f - (paint.descent() + paint.ascent()) / 2.0f,
                    paint)

            return bitmap
        }

        /**
         * Get a representative character for the given URL.
         *
         * For example this method will return "f" for "http://m.facebook.com/foobar".
         */
        @VisibleForTesting fun getRepresentativeCharacter(url: String?): String {
            val firstChar = getRepresentativeSnippet(url)?.find { it.isLetterOrDigit() }?.toUpperCase()
            return (firstChar ?: DEFAULT_ICON_CHAR).toString()
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
