/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ext

import android.content.Context
import androidx.annotation.ColorInt
import androidx.annotation.ColorRes
import androidx.core.content.ContextCompat
import org.mozilla.fenix.ThemeManager

/**
 * Returns the color resource corresponding to the attribute.
 */
@ColorRes
fun Int.getColorIntFromAttr(context: Context): Int = ThemeManager.resolveAttribute(this, context)

/**
 * Returns the color int corresponding to the attribute.
 */
@ColorInt
fun Int.getColorFromAttr(context: Context): Int = ContextCompat.getColor(context, this.getColorIntFromAttr(context))
