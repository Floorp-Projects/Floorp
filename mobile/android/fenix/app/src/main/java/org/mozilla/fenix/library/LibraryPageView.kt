/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library

import android.content.Context
import android.graphics.ColorFilter
import android.graphics.PorterDuff
import android.graphics.PorterDuffColorFilter
import android.view.ViewGroup
import android.widget.ActionMenuView
import android.widget.ImageButton
import androidx.annotation.ColorRes
import androidx.appcompat.view.menu.ActionMenuItemView
import androidx.appcompat.widget.Toolbar
import androidx.core.content.ContextCompat
import androidx.core.view.forEach
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.asActivity

open class LibraryPageView(
    container: ViewGroup
) {
    protected val context: Context = container.context
    protected val activity = context.asActivity()

    /**
     * Adjust the colors of the [Toolbar] on the top of the screen.
     */
    protected fun setToolbarColors(@ColorRes foregroundRes: Int, @ColorRes backgroundRes: Int) {
        val toolbar = activity?.findViewById<Toolbar>(R.id.navigationToolbar)

        val foreground = ContextCompat.getColor(context, foregroundRes)
        val background = ContextCompat.getColor(context, backgroundRes)

        toolbar?.apply {
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
}
