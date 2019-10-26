/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ext

import android.graphics.ColorFilter
import android.graphics.PorterDuff
import android.graphics.PorterDuffColorFilter
import android.widget.ActionMenuView
import android.widget.ImageButton
import androidx.annotation.ColorInt
import androidx.appcompat.view.menu.ActionMenuItemView
import androidx.appcompat.widget.Toolbar
import androidx.core.view.forEach

/**
 * Adjust the colors of the [Toolbar] on the top of the screen.
 */
fun Toolbar.setToolbarColors(@ColorInt foreground: Int, @ColorInt background: Int) {
    apply {
        setBackgroundColor(background)
        setTitleTextColor(foreground)

        val colorFilter = PorterDuffColorFilter(foreground, PorterDuff.Mode.SRC_IN)
        overflowIcon?.colorFilter = colorFilter
        forEach { child ->
            when (child) {
                is ImageButton -> child.drawable.colorFilter = colorFilter
                is ActionMenuView -> themeActionMenuView(child, colorFilter)
            }
        }
    }
}

private fun themeActionMenuView(item: ActionMenuView, colorFilter: ColorFilter) {
    item.forEach { innerChild ->
        if (innerChild is ActionMenuItemView) {
            innerChild.compoundDrawables.forEach { drawable ->
                item.post { drawable?.colorFilter = colorFilter }
            }
        }
    }
}
