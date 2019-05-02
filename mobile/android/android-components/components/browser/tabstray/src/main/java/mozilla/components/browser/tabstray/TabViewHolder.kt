/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.tabstray

import android.content.res.ColorStateList
import android.view.View
import android.widget.ImageView
import android.widget.TextView
import androidx.appcompat.widget.AppCompatImageButton
import androidx.cardview.widget.CardView
import androidx.recyclerview.widget.RecyclerView
import mozilla.components.browser.session.Session
import mozilla.components.browser.tabstray.thumbnail.TabThumbnailView
import mozilla.components.concept.tabstray.TabsTray
import mozilla.components.support.base.observer.Observable

/**
 * A RecyclerView ViewHolder implementation for "tab" items.
 */
class TabViewHolder(
    itemView: View,
    private val tabsTray: BrowserTabsTray
) : RecyclerView.ViewHolder(itemView), Session.Observer {
    private val cardView: CardView = (itemView as CardView).apply {
        elevation = tabsTray.styling.itemElevation
    }
    private val iconView: ImageView = itemView.findViewById(R.id.mozac_browser_tabstray_icon)
    private val tabView: TextView = itemView.findViewById(R.id.mozac_browser_tabstray_url)
    private val closeView: AppCompatImageButton = itemView.findViewById(R.id.mozac_browser_tabstray_close)
    private val thumbnailView: TabThumbnailView = itemView.findViewById(R.id.mozac_browser_tabstray_thumbnail)

    internal var session: Session? = null

    /**
     * Displays the data of the given session and notifies the given observable about events.
     */
    fun bind(session: Session, isSelected: Boolean, observable: Observable<TabsTray.Observer>) {
        this.session = session.also { it.register(this) }

        val title = if (session.title.isNotEmpty()) {
            session.title
        } else {
            session.url
        }

        tabView.text = title

        itemView.setOnClickListener {
            observable.notifyObservers { onTabSelected(session) }
        }

        closeView.setOnClickListener {
            observable.notifyObservers { onTabClosed(session) }
        }

        if (isSelected) {
            tabView.setTextColor(tabsTray.styling.selectedItemTextColor)
            cardView.setCardBackgroundColor(tabsTray.styling.selectedItemBackgroundColor)
            closeView.imageTintList = ColorStateList.valueOf(tabsTray.styling.selectedItemTextColor)
        } else {
            tabView.setTextColor(tabsTray.styling.itemTextColor)
            cardView.setCardBackgroundColor(tabsTray.styling.itemBackgroundColor)
            closeView.imageTintList = ColorStateList.valueOf(tabsTray.styling.itemTextColor)
        }

        thumbnailView.setImageBitmap(session.thumbnail)

        iconView.setImageBitmap(session.icon)
    }

    /**
     * The attached view no longer needs to display any data.
     */
    fun unbind() {
        session?.unregister(this)
    }

    override fun onUrlChanged(session: Session, url: String) {
        tabView.text = url
    }
}
