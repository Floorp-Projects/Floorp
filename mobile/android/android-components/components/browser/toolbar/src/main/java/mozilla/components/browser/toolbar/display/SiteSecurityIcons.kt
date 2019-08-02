/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.toolbar.display

import android.content.Context
import android.graphics.BlendMode
import android.graphics.BlendModeColorFilter
import android.graphics.ColorFilter
import android.graphics.PorterDuff
import android.graphics.PorterDuffColorFilter
import android.graphics.drawable.Drawable
import android.os.Build
import android.os.Build.VERSION.SDK_INT
import androidx.annotation.ColorInt
import mozilla.components.ui.icons.R

/**
 * Specifies icons to display in the toolbar representing the security of the current website.
 *
 * @property insecure Icon to display for HTTP sites.
 * @property secure Icon to display for HTTPS sites.
 */
data class SiteSecurityIcons(
    val insecure: Drawable?,
    val secure: Drawable?
) {

    /**
     * Returns an instance of [SiteSecurityIcons] with a color filter applied to each icon.
     */
    fun withColorFilter(insecureColorFilter: ColorFilter, secureColorFilter: ColorFilter): SiteSecurityIcons {
        return copy(
            insecure = insecure?.apply { colorFilter = insecureColorFilter },
            secure = secure?.apply { colorFilter = secureColorFilter }
        )
    }

    /**
     * Returns an instance of [SiteSecurityIcons] with a color tint applied to each icon.
     */
    fun withColorFilter(@ColorInt insecureColor: Int, @ColorInt secureColor: Int): SiteSecurityIcons {
        val insecureColorFilter: ColorFilter = if (SDK_INT >= Build.VERSION_CODES.Q) {
            BlendModeColorFilter(insecureColor, BlendMode.SRC_IN)
        } else {
            PorterDuffColorFilter(insecureColor, PorterDuff.Mode.SRC_IN)
        }

        val secureColorFilter: ColorFilter = if (SDK_INT >= Build.VERSION_CODES.Q) {
            BlendModeColorFilter(secureColor, BlendMode.SRC_IN)
        } else {
            PorterDuffColorFilter(secureColor, PorterDuff.Mode.SRC_IN)
        }

        return withColorFilter(insecureColorFilter, secureColorFilter)
    }

    companion object {
        fun getDefaultSecurityIcons(context: Context, @ColorInt color: Int): SiteSecurityIcons {
            return SiteSecurityIcons(
                insecure = context.getDrawable(R.drawable.mozac_ic_globe),
                secure = context.getDrawable(R.drawable.mozac_ic_lock)
            ).withColorFilter(color, color)
        }
    }
}
