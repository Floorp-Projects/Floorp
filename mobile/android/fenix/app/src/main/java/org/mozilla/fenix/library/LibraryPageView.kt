/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library

import android.content.Context
import android.graphics.ColorFilter
import android.graphics.PorterDuff
import android.graphics.PorterDuffColorFilter
import android.view.View
import android.view.ViewGroup
import android.widget.ActionMenuView
import android.widget.ImageButton
import androidx.annotation.ColorInt
import androidx.appcompat.view.menu.ActionMenuItemView
import androidx.appcompat.widget.Toolbar
import androidx.core.content.ContextCompat
import androidx.core.view.children
import androidx.core.view.forEach
import androidx.recyclerview.widget.RecyclerView
import kotlinx.android.extensions.LayoutContainer
import kotlinx.android.synthetic.main.library_site_item.view.*
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.asActivity
import org.mozilla.fenix.ext.getColorFromAttr

open class LibraryPageView(
    override val containerView: ViewGroup
) : LayoutContainer {
    protected val context: Context inline get() = containerView.context
    protected val activity = context.asActivity()

    protected fun setUiForNormalMode(
        title: String?,
        libraryItemsList: RecyclerView
    ) {
        activity?.title = title
        setToolbarColors(
            context.getColorFromAttr(R.attr.primaryText),
            context.getColorFromAttr(R.attr.foundation)
        )
        libraryItemsList.children.forEach {
                item -> item.overflow_menu.visibility = View.VISIBLE
        }
    }

    protected fun setUiForSelectingMode(
        title: String?,
        libraryItemsList: RecyclerView
    ) {
        activity?.title = title
        setToolbarColors(
            ContextCompat.getColor(context, R.color.white_color),
            context.getColorFromAttr(R.attr.accentHighContrast)
        )
        libraryItemsList.children.forEach {
            item -> item.overflow_menu.visibility = View.INVISIBLE
        }
    }

    /**
     * Adjust the colors of the [Toolbar] on the top of the screen.
     */
    private fun setToolbarColors(@ColorInt foreground: Int, @ColorInt background: Int) {
        val toolbar = activity?.findViewById<Toolbar>(R.id.navigationToolbar)

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
