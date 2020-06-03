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
        object Back : Item()
        object Forward : Item()
        object Reload : Item()
        object Stop : Item()
        object OpenInFenix : Item()
        object SaveToCollection : Item()
        object AddToTopSites : Item()
        object InstallToHomeScreen : Item()
        object AddToHomeScreen : Item()
        object SyncedTabs : Item()
        object AddonsManager : Item()
        object Quit : Item()
        data class ReaderMode(val isChecked: Boolean) : Item()
        object OpenInApp : Item()
        object Bookmark : Item()
        object ReaderModeAppearance : Item()
        object Bookmarks : Item()
        object History : Item()
    }

    val menuBuilder: BrowserMenuBuilder
    val menuToolbar: BrowserMenuItemToolbar
}
