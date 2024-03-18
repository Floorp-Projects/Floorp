/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:Suppress("NOTHING_TO_INLINE") // Aliases to other public APIs.

package mozilla.components.support.ktx.android.view

import android.graphics.drawable.Drawable
import android.widget.TextView

/**
 * Sets the [Drawable]s (if any) to appear to the start of, above, to the end of,
 * and below the text. Use `null` if you do not want a Drawable there.
 * The Drawables must already have had [Drawable.setBounds] called.
 *
 * Calling this method will overwrite any Drawables previously set using
 * [TextView.setCompoundDrawables] or related methods.
 */
inline fun TextView.putCompoundDrawablesRelative(
    start: Drawable? = null,
    top: Drawable? = null,
    end: Drawable? = null,
    bottom: Drawable? = null,
) = setCompoundDrawablesRelative(start, top, end, bottom)

/**
 *
 * Sets the [Drawable]s (if any) to appear to the start of, above, to the end of,
 * and below the text. Use `null` if you do not want a Drawable there.
 * The Drawables' bounds will be set to their intrinsic bounds.
 *
 * Calling this method will overwrite any Drawables previously set using
 * [TextView.setCompoundDrawables] or related methods.
 */
inline fun TextView.putCompoundDrawablesRelativeWithIntrinsicBounds(
    start: Drawable? = null,
    top: Drawable? = null,
    end: Drawable? = null,
    bottom: Drawable? = null,
) = setCompoundDrawablesRelativeWithIntrinsicBounds(start, top, end, bottom)
