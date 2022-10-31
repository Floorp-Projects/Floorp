/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.menu

import android.content.res.ColorStateList
import androidx.annotation.ColorInt
import androidx.annotation.Px

/**
 * Declare custom styles for a menu.
 *
 * @property backgroundColor Custom background color for the menu.
 * @property minWidth Custom minimum width for the menu.
 * @property maxWidth Custom maximum width for the menu.
 */
data class MenuStyle(
    val backgroundColor: ColorStateList? = null,
    @Px val minWidth: Int? = null,
    @Px val maxWidth: Int? = null,
) {
    constructor(
        @ColorInt backgroundColor: Int,
        @Px minWidth: Int? = null,
        @Px maxWidth: Int? = null,
    ) : this(
        backgroundColor = ColorStateList.valueOf(backgroundColor),
        minWidth = minWidth,
        maxWidth = maxWidth,
    )
}
