/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library

import android.content.Context
import android.util.AttributeSet
import android.view.LayoutInflater
import android.widget.ImageButton
import android.widget.ImageView
import android.widget.TextView
import androidx.constraintlayout.widget.ConstraintLayout
import androidx.core.view.isVisible
import kotlinx.android.synthetic.main.library_site_item.view.*
import mozilla.components.browser.menu.BrowserMenu
import mozilla.components.browser.menu.BrowserMenuBuilder
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.increaseTapArea
import org.mozilla.fenix.ext.loadIntoView

/**
 * Interactor for items that can be selected on the bookmarks and history screens.
 */
interface SelectionInteractor<T> {
    /**
     * Called when an item is tapped to open it.
     * @param item the tapped item to open.
     */
    fun open(item: T)

    /**
     * Called when an item is long pressed and selection mode is started,
     * or when selection mode has already started an an item is tapped.
     * @param item the item to select.
     */
    fun select(item: T)

    /**
     * Called when a selected item is tapped in selection mode and should no longer be selected.
     * @param item the item to deselect.
     */
    fun deselect(item: T)
}

interface SelectionHolder<T> {
    val selectedItems: Set<T>
}

interface LibraryItemMenu {
    val menuBuilder: BrowserMenuBuilder
}

class LibrarySiteItemView @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = 0,
    defStyleRes: Int = 0
) : ConstraintLayout(context, attrs, defStyleAttr, defStyleRes) {

    val titleView: TextView get() = title

    val urlView: TextView get() = url

    val iconView: ImageView get() = favicon

    val overflowView: ImageButton get() = overflow_menu

    init {
        LayoutInflater.from(context).inflate(R.layout.library_site_item, this, true)

        overflow_menu.increaseTapArea(OVERFLOW_EXTRA_DIPS)
    }

    /**
     * Change visibility of parts of this view based on what type of item is being represented.
     */
    fun displayAs(mode: ItemType) {
        favicon.isVisible = mode != ItemType.SEPARATOR
        title.isVisible = mode != ItemType.SEPARATOR
        url.isVisible = mode == ItemType.SITE
        overflow_menu.isVisible = mode != ItemType.SEPARATOR
        separator.isVisible = mode == ItemType.SEPARATOR
        isClickable = mode != ItemType.SEPARATOR
        isFocusable = mode != ItemType.SEPARATOR
    }

    /**
     * Changes the icon to show a check mark if [isSelected]
     */
    fun changeSelected(isSelected: Boolean) {
        icon.displayedChild = if (isSelected) 1 else 0
    }

    fun loadFavicon(url: String) {
        context.components.core.icons.loadIntoView(favicon, url)
    }

    fun attachMenu(menu: LibraryItemMenu) {
        overflow_menu.setOnClickListener {
            menu.menuBuilder.build(context).show(
                anchor = it,
                orientation = BrowserMenu.Orientation.DOWN
            )
        }
    }

    fun <T> setSelectionInteractor(item: T, holder: SelectionHolder<T>, interactor: SelectionInteractor<T>) {
        setOnClickListener {
            val selected = holder.selectedItems
            when {
                selected.isEmpty() -> interactor.open(item)
                item in selected -> interactor.deselect(item)
                else -> interactor.select(item)
            }
        }

        setOnLongClickListener {
            if (holder.selectedItems.isEmpty()) {
                interactor.select(item)
                true
            } else {
                false
            }
        }

        favicon.setOnClickListener {
            if (item in holder.selectedItems) {
                interactor.deselect(item)
            } else {
                interactor.select(item)
            }
        }
    }

    enum class ItemType {
        SITE, FOLDER, SEPARATOR;
    }

    companion object {
        private const val OVERFLOW_EXTRA_DIPS = 16
    }
}
