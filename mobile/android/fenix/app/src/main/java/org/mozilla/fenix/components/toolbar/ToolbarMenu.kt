/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.toolbar

import mozilla.components.browser.menu.BrowserMenuBuilder
import mozilla.components.browser.menu.item.BrowserMenuItemToolbar

interface ToolbarMenu {
    sealed class Item {
        object Help : Item()
        object Settings : Item()
        object Library : Item()
        data class RequestDesktop(val isChecked: Boolean) : Item()
        object FindInPage : Item()
        object NewPrivateTab : Item()
        object NewTab : Item()
        object Share : Item()
        object Back : Item()
        object Forward : Item()
        object Reload : Item()
        object Stop : Item()
        object ReportIssue : Item()
        object OpenInFenix : Item()
        object SaveToCollection : Item()
        object AddToFirefoxHome : Item()
        object AddToHomeScreen : Item()
        object AddonsManager : Item()
        object Quit : Item()
        data class ReaderMode(val isChecked: Boolean) : Item()
        object OpenInApp : Item()
        object Bookmark : Item()
        object ReaderModeAppearance : Item()
    }

    val menuBuilder: BrowserMenuBuilder
    val menuToolbar: BrowserMenuItemToolbar
}
