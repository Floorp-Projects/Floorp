/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.recenttabs.view

import android.view.View
import kotlinx.android.synthetic.main.recent_tabs_list_row.*
import mozilla.components.browser.icons.BrowserIcons
import mozilla.components.browser.state.state.ContentState
import mozilla.components.browser.state.state.TabSessionState
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.loadIntoView
import org.mozilla.fenix.home.recenttabs.interactor.RecentTabInteractor
import org.mozilla.fenix.utils.view.ViewHolder

/**
 * View holder for a recent tab item.
 *
 * @param interactor [RecentTabInteractor] which will have delegated to all user interactions.
 * @param icons an instance of [BrowserIcons] for rendering the sites icon if one isn't found
 * in [ContentState.icon].
 */
class RecentTabViewHolder(
    view: View,
    private val interactor: RecentTabInteractor,
    private val icons: BrowserIcons = view.context.components.core.icons
) : ViewHolder(view) {

    fun bindTab(tab: TabSessionState): View {
        // A page may take a while to retrieve a title, so let's show the url until we get one.
        recent_tab_title.text = if (tab.content.title.isNotEmpty()) {
            tab.content.title
        } else {
            tab.content.url
        }

        if (tab.content.icon != null) {
            recent_tab_icon.setImageBitmap(tab.content.icon)
        } else {
            icons.loadIntoView(recent_tab_icon, tab.content.url)
        }
        recent_tab_icon.setImageBitmap(tab.content.icon)

        itemView.setOnClickListener {
            interactor.onRecentTabClicked(tab.id)
        }

        return itemView
    }

    companion object {
        const val LAYOUT_ID = R.layout.recent_tabs_list_row
    }
}
