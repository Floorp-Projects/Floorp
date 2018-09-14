/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.tabstray

import android.graphics.Color
import android.support.v4.content.ContextCompat
import android.support.v7.widget.CardView
import android.support.v7.widget.RecyclerView
import android.view.View
import android.widget.ImageButton
import android.widget.ImageView
import android.widget.TextView
import mozilla.components.browser.session.Session
import mozilla.components.concept.tabstray.TabsTray
import mozilla.components.support.base.observer.Observable

/**
 * A RecyclerView ViewHolder implementation for "tab" items.
 */
class TabViewHolder(
    itemView: View
) : RecyclerView.ViewHolder(itemView), Session.Observer {
    private val cardView: CardView = itemView as CardView
    private val tabView: TextView = itemView.findViewById(R.id.mozac_browser_tabstray_url)
    private val closeView: ImageButton = itemView.findViewById(R.id.mozac_browser_tabstray_close)
    private val thumbnailView: ImageView = itemView.findViewById(R.id.mozac_browser_tabstray_thumbnail)
    private val selectedColor = ContextCompat.getColor(
        itemView.context, mozilla.components.ui.colors.R.color.photonBlue40
    )

    private var session: Session? = null

    /**
     * Displays the data of the given session and notifies the given observable about events.
     */
    fun bind(session: Session, isSelected: Boolean, observable: Observable<TabsTray.Observer>) {
        this.session = session.also { it.register(this) }

        tabView.text = session.url

        itemView.setOnClickListener {
            observable.notifyObservers { onTabSelected(session) }
        }

        closeView.setOnClickListener {
            observable.notifyObservers { onTabClosed(session) }
        }

        if (isSelected) {
            cardView.setCardBackgroundColor(selectedColor)
        } else {
            cardView.setCardBackgroundColor(Color.WHITE)
        }

        if (session.thumbnail != null) {
            thumbnailView.setImageBitmap(session.thumbnail)
        }
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
