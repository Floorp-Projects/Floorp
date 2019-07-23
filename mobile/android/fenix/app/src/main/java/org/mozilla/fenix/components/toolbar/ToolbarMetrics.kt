/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.toolbar

import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.components.metrics.MetricController

// This method triggers the complexity warning. However it's actually not that hard to understand.
@SuppressWarnings("ComplexMethod")
fun trackToolbarItemInteraction(metrics: MetricController, action: SearchAction.ToolbarMenuItemTapped) {
    val item = when (action.item) {
        ToolbarMenu.Item.Back -> Event.BrowserMenuItemTapped.Item.BACK
        ToolbarMenu.Item.Forward -> Event.BrowserMenuItemTapped.Item.FORWARD
        ToolbarMenu.Item.Reload -> Event.BrowserMenuItemTapped.Item.RELOAD
        ToolbarMenu.Item.Stop -> Event.BrowserMenuItemTapped.Item.STOP
        ToolbarMenu.Item.Settings -> Event.BrowserMenuItemTapped.Item.SETTINGS
        ToolbarMenu.Item.Library -> Event.BrowserMenuItemTapped.Item.LIBRARY
        is ToolbarMenu.Item.RequestDesktop -> if (action.item.isChecked) {
            Event.BrowserMenuItemTapped.Item.DESKTOP_VIEW_ON
        } else {
            Event.BrowserMenuItemTapped.Item.DESKTOP_VIEW_OFF
        }
        ToolbarMenu.Item.NewPrivateTab -> Event.BrowserMenuItemTapped.Item.NEW_PRIVATE_TAB
        ToolbarMenu.Item.FindInPage -> Event.BrowserMenuItemTapped.Item.FIND_IN_PAGE
        ToolbarMenu.Item.ReportIssue -> Event.BrowserMenuItemTapped.Item.REPORT_SITE_ISSUE
        ToolbarMenu.Item.Help -> Event.BrowserMenuItemTapped.Item.HELP
        ToolbarMenu.Item.NewTab -> Event.BrowserMenuItemTapped.Item.NEW_TAB
        ToolbarMenu.Item.OpenInFenix -> Event.BrowserMenuItemTapped.Item.OPEN_IN_FENIX
        ToolbarMenu.Item.Share -> Event.BrowserMenuItemTapped.Item.SHARE
        ToolbarMenu.Item.SaveToCollection -> Event.BrowserMenuItemTapped.Item.SAVE_TO_COLLECTION
    }

    metrics.track(Event.BrowserMenuItemTapped(item))
}
