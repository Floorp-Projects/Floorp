/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.utils

import android.content.Context
import android.graphics.drawable.Drawable
import androidx.annotation.ColorInt
import androidx.annotation.DrawableRes
import androidx.core.graphics.drawable.DrawableCompat

object DrawableUtils {
    /**
     * Return a tinted drawable object associated with a particular resource ID.
     */
    fun loadAndTintDrawable(context: Context, @DrawableRes resourceId: Int, @ColorInt color: Int): Drawable {
        val drawable = context.resources.getDrawable(resourceId, context.theme)
        val wrapped = DrawableCompat.wrap(drawable.mutate())
        DrawableCompat.setTint(wrapped, color)
        return wrapped
    }
}
