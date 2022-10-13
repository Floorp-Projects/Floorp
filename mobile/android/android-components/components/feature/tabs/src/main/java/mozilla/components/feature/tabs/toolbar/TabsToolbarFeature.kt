/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.tabs.toolbar

import androidx.lifecycle.LifecycleOwner
import mozilla.components.browser.state.selector.findCustomTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.toolbar.Toolbar
import mozilla.components.ui.tabcounter.TabCounterMenu

/**
 * Feature implementation for connecting a tabs tray implementation with a toolbar implementation.
 *
 * @param countBasedOnSelectedTabType if true the count is based on the selected tab i.e. if a
 * private tab is selected private tabs will be counter, otherwise normal tabs. If false, all
 * tabs will be counted.
 */
// TODO Refactor or remove this feature: https://github.com/mozilla-mobile/android-components/issues/9129
@Suppress("LongParameterList")
class TabsToolbarFeature(
    toolbar: Toolbar,
    store: BrowserStore,
    sessionId: String? = null,
    lifecycleOwner: LifecycleOwner,
    showTabs: () -> Unit,
    tabCounterMenu: TabCounterMenu? = null,
    countBasedOnSelectedTabType: Boolean = true,
) {
    init {
        run {
            // this feature is not used for Custom Tabs
            if (sessionId != null && store.state.findCustomTab(sessionId) != null) return@run

            val tabsAction = TabCounterToolbarButton(
                lifecycleOwner = lifecycleOwner,
                showTabs = showTabs,
                store = store,
                menu = tabCounterMenu,
                countBasedOnSelectedTabType = countBasedOnSelectedTabType,
            )
            toolbar.addBrowserAction(tabsAction)
        }
    }
}
