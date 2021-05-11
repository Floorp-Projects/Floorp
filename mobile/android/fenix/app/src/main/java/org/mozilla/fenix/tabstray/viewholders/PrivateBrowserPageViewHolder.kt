/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray.viewholders

import android.view.View
import org.mozilla.fenix.R
import org.mozilla.fenix.tabstray.TabsTrayInteractor
import org.mozilla.fenix.tabstray.TabsTrayStore
import org.mozilla.fenix.tabstray.browser.BrowserTrayList.BrowserTabType.PRIVATE

/**
 * View holder for the private tabs tray list.
 */
class PrivateBrowserPageViewHolder(
    containerView: View,
    store: TabsTrayStore,
    interactor: TabsTrayInteractor,
    currentTabIndex: Int
) : AbstractBrowserPageViewHolder(
    containerView,
    store,
    interactor,
    currentTabIndex
) {

    init {
        trayList.browserTabType = PRIVATE
    }

    override val emptyStringText: String
        get() = itemView.resources.getString(R.string.no_private_tabs_description)

    companion object {
        const val LAYOUT_ID = R.layout.private_browser_tray_list
    }
}
