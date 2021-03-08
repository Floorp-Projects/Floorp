/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.toolbar

import mozilla.components.browser.menu.BrowserMenuBuilder
import mozilla.components.browser.menu.item.BrowserMenuItemToolbar

interface ToolbarMenu {
    sealed class Item {
        object Settings : Item()
        data class RequestDesktop(val isChecked: Boolean) : Item()
        object FindInPage : Item()
        object Share : Item()
        data class Back(val viewHistory: Boolean) : Item()
        data class Forward(val viewHistory: Boolean) : Item()
        data class Reload(val bypassCache: Boolean) : Item()
        object Stop : Item()
        object OpenInFenix : Item()
        object SaveToCollection : Item()
        object AddToTopSites : Item()
        object InstallToHomeScreen : Item()
        object AddToHomeScreen : Item()
        object SyncedTabs : Item()
        object AddonsManager : Item()
        object Quit : Item()
        object OpenInApp : Item()
        object Bookmark : Item()
        object CustomizeReaderView : Item()
        object Bookmarks : Item()
        object History : Item()
        object Downloads : Item()
        object NewTab : Item()
    }

    val menuBuilder: BrowserMenuBuilder
    val menuToolbar: BrowserMenuItemToolbar
}
