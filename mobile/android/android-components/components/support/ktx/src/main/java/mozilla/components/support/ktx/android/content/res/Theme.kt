/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.ktx.android.content.res

import android.content.res.Resources
import android.util.TypedValue
import androidx.annotation.AnyRes
import androidx.annotation.AttrRes

/**
 * Resolves the resource ID corresponding to the given attribute.
 *
 * @sample
 * context.theme.resolveAttribute(R.attr.textColor) == R.color.light_text_color
 */
@AnyRes
fun Resources.Theme.resolveAttribute(@AttrRes attribute: Int): Int {
    val outValue = TypedValue()
    resolveAttribute(attribute, outValue, true)
    return outValue.resourceId
}
