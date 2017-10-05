/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.shortcut

import android.annotation.TargetApi
import android.content.Context
import android.graphics.Bitmap
import android.graphics.BitmapFactory
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.graphics.drawable.AdaptiveIconDrawable
import android.graphics.drawable.BitmapDrawable
import android.graphics.drawable.ColorDrawable
import android.net.Uri
import android.support.annotation.VisibleForTesting
import android.support.v4.content.ContextCompat
import android.util.TypedValue

import org.mozilla.focus.R
import org.mozilla.focus.utils.UrlUtils

class IconGenerator {

    companion object {
        private val ICON_SIZE_PRE_OREO = 96
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

            val icon = generateIcon(context, url, ICON_SIZE_PRE_OREO)

            val drawX = shape.width / 2f - icon.width / 2f
            val drawY = shape.height / 2f - icon.height / 2f

            val canvas = Canvas(shape)
            canvas.drawBitmap(icon, drawX, drawY, Paint())

            return shape
        }

        /**
         * Generates a launcher icon for versions of Android that support Adaptive Icons (Oreo+):
         * https://developer.android.com/guide/practices/ui_guidelines/icon_design_adaptive.html
         *
         * A bitmap created from this drawable (e.g. Drawable.toBitmap) is expected to be used with
         * [android.support.v4.graphics.drawable.IconCompat.createWithBitmap], not
         * [android.support.v4.graphics.drawable.IconCompat.createWithAdaptiveBitmap]. Why? createWithAdaptiveBitmap
         * will apply the platform's adaptive icon formatting to the given bitmap so it assumes you have a bitmap that
         * hasn't been formatted already. However, when a bitmap is created with [AdaptiveIconDrawable], the formatting
         * is applied so it'd be incorrect for us to use createWithAdaptiveBitmap, which would apply the formatting
         * again.
         *
         * Unfortunately, we also can't just pass our legacy icon into createWithAdaptiveBitmap because the icon
         * background has the corners removed and some device might display icons that have less of the corner missing
         * than we do. In that case, we'd be unable to add the corners back and the icon wouldn't conform.
         */
        @TargetApi(26)
        private fun generateAdaptiveLauncherIcon(context: Context, url: String?): AdaptiveIconDrawable {
            val res = context.resources
            val backgroundColor = ColorDrawable(ContextCompat.getColor(context, R.color.add_to_homescreen_icon_background))

            val adaptiveIconDimen = res.getDimensionPixelSize(R.dimen.adaptive_icon_drawable_dimen)
            val foregroundLetter = BitmapDrawable(res, generateIcon(context, url, adaptiveIconDimen))

            return AdaptiveIconDrawable(backgroundColor, foregroundLetter)
        }

        /**
         * Generate an icon for this website based on the URL.
         */
        private fun generateIcon(context: Context, url: String?, iconSize: Int): Bitmap {
            val bitmap = Bitmap.createBitmap(iconSize, iconSize, Bitmap.Config.ARGB_8888)
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
        @VisibleForTesting internal fun getRepresentativeCharacter(url: String?): String {
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
