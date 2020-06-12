/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.tabstray

import android.content.res.ColorStateList
import android.view.View
import android.widget.ImageView
import android.widget.TextView
import androidx.appcompat.widget.AppCompatImageButton
import androidx.recyclerview.widget.RecyclerView
import mozilla.components.browser.tabstray.thumbnail.TabThumbnailView
import mozilla.components.concept.tabstray.Tab
import mozilla.components.concept.tabstray.TabsTray
import mozilla.components.support.base.observer.Observable
import mozilla.components.support.images.loader.ImageLoader
import mozilla.components.support.ktx.kotlin.tryGetHostFromUrl

/**
 * An abstract ViewHolder implementation for "tab" items.
 */
abstract class TabViewHolder(view: View) : RecyclerView.ViewHolder(view) {
    abstract var tab: Tab?

    /**
     * Binds the ViewHolder to the `Tab`.
     * @param tab the `Tab` used to bind the viewHolder.
     * @param isSelected boolean to describe whether or not the `Tab` is selected.
     * @param observable message bus to pass events to Observers of the TabsTray.
     */
    abstract fun bind(tab: Tab, isSelected: Boolean, observable: Observable<TabsTray.Observer>)
}

/**
 * The default implementation of `TabViewHolder`
 */
class DefaultTabViewHolder(
    itemView: View,
    private val tabsTray: BrowserTabsTray,
    private val thumbnailLoader: ImageLoader? = null
) : TabViewHolder(itemView) {
    private val iconView: ImageView? = itemView.findViewById(R.id.mozac_browser_tabstray_icon)
    private val titleView: TextView = itemView.findViewById(R.id.mozac_browser_tabstray_title)
    private val closeView: AppCompatImageButton = itemView.findViewById(R.id.mozac_browser_tabstray_close)
    private val thumbnailView: TabThumbnailView = itemView.findViewById(R.id.mozac_browser_tabstray_thumbnail)
    private val urlView: TextView? = itemView.findViewById(R.id.mozac_browser_tabstray_url)

    override var tab: Tab? = null

    /**
     * Displays the data of the given session and notifies the given observable about events.
     */
    override fun bind(tab: Tab, isSelected: Boolean, observable: Observable<TabsTray.Observer>) {
        this.tab = tab

        val title = if (tab.title.isNotEmpty()) {
            tab.title
        } else {
            tab.url
        }

        titleView.text = title
        urlView?.text = tab.url.tryGetHostFromUrl()

        itemView.setOnClickListener {
            observable.notifyObservers { onTabSelected(tab) }
        }

        closeView.setOnClickListener {
            observable.notifyObservers { onTabClosed(tab) }
        }

        if (isSelected) {
            titleView.setTextColor(tabsTray.styling.selectedItemTextColor)
            itemView.setBackgroundColor(tabsTray.styling.selectedItemBackgroundColor)
            closeView.imageTintList = ColorStateList.valueOf(tabsTray.styling.selectedItemTextColor)
        } else {
            titleView.setTextColor(tabsTray.styling.itemTextColor)
            itemView.setBackgroundColor(tabsTray.styling.itemBackgroundColor)
            closeView.imageTintList = ColorStateList.valueOf(tabsTray.styling.itemTextColor)
        }

        // In the final else case, we have no cache or fresh screenshot; do nothing instead of clearing the image.
        if (thumbnailLoader != null && tab.thumbnail == null) {
            thumbnailLoader.loadIntoView(thumbnailView, tab.id)
        } else if (tab.thumbnail != null) {
            thumbnailView.setImageBitmap(tab.thumbnail)
        }

        iconView?.setImageBitmap(tab.icon)
    }
}
