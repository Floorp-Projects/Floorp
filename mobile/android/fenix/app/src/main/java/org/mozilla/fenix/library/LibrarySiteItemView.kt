/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library

import android.content.Context
import android.view.LayoutInflater
import android.widget.ImageButton
import android.widget.ImageView
import android.widget.TextView
import androidx.constraintlayout.widget.ConstraintLayout
import androidx.core.view.isVisible
import kotlinx.android.synthetic.main.library_site_item.view.*
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.increaseTapArea
import org.mozilla.fenix.ext.loadIntoView

class LibrarySiteItemView(
    context: Context
) : ConstraintLayout(context) {

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

    enum class ItemType {
        SITE, FOLDER, SEPARATOR;
    }

    companion object {
        private const val OVERFLOW_EXTRA_DIPS = 16
    }
}
