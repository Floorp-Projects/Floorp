/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.tabs.toolbar

import android.content.Context
import mozilla.components.concept.toolbar.Toolbar
import mozilla.components.feature.tabs.R

/**
 * Feature implementation for connecting a tabs tray implementation with a toolbar implementation.
 */

class TabsToolbarFeature(
    context: Context,
    toolbar: Toolbar,
    showTabs: () -> Unit
) {
    init {
        val tabsAction = Toolbar.ActionButton(
            mozilla.components.ui.icons.R.drawable.mozac_ic_tab,
            context.getString(R.string.mozac_feature_tabs_toolbar_tabs_button)) {
            showTabs.invoke()
        }

        toolbar.addBrowserAction(tabsAction)
    }
}
