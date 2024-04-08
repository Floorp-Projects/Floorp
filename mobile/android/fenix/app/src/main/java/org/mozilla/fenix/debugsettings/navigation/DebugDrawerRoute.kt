/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.debugsettings.navigation

import androidx.annotation.StringRes
import androidx.compose.runtime.Composable
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.storage.LoginsStorage
import org.mozilla.fenix.R
import org.mozilla.fenix.debugsettings.logins.LoginsTools
import org.mozilla.fenix.debugsettings.store.DebugDrawerAction
import org.mozilla.fenix.debugsettings.store.DebugDrawerStore
import org.mozilla.fenix.debugsettings.tabs.TabTools as TabToolsScreen

/**
 * The navigation routes for screens within the Debug Drawer.
 *
 * @property route The unique route used to navigate to the destination. This string can also contain
 * optional parameters for arguments or deep linking.
 * @property title The string ID of the destination's title.
 */
enum class DebugDrawerRoute(val route: String, @StringRes val title: Int) {
    /**
     * The navigation route for [TabToolsScreen].
     */
    TabTools(
        route = "tab_tools",
        title = R.string.debug_drawer_tab_tools_title,
    ),
    Logins(
        route = "logins",
        title = R.string.debug_drawer_logins_title,
    ),
    ;

    companion object {
        /**
         * Transforms the values of [DebugDrawerRoute] into a list of [DebugDrawerDestination]s.
         *
         * @param debugDrawerStore [DebugDrawerStore] used to dispatch navigation actions.
         * @param browserStore [BrowserStore] used to access [BrowserState].
         * @param loginsStorage [LoginsStorage] used to access logins for [LoginsScreen].
         * @param inactiveTabsEnabled Whether the inactive tabs feature is enabled.
         */
        fun generateDebugDrawerDestinations(
            debugDrawerStore: DebugDrawerStore,
            browserStore: BrowserStore,
            loginsStorage: LoginsStorage,
            inactiveTabsEnabled: Boolean,
        ): List<DebugDrawerDestination> =
            DebugDrawerRoute.values().map { debugDrawerRoute ->
                val onClick: () -> Unit
                val content: @Composable () -> Unit
                when (debugDrawerRoute) {
                    TabTools -> {
                        onClick = {
                            debugDrawerStore.dispatch(DebugDrawerAction.NavigateTo.TabTools)
                        }
                        content = {
                            TabToolsScreen(
                                store = browserStore,
                                inactiveTabsEnabled = inactiveTabsEnabled,
                            )
                        }
                    }

                    Logins -> {
                        onClick = {
                            debugDrawerStore.dispatch(DebugDrawerAction.NavigateTo.Logins)
                        }
                        content = {
                            LoginsTools(
                                browserStore = browserStore,
                                loginsStorage = loginsStorage,
                            )
                        }
                    }
                }

                DebugDrawerDestination(
                    route = debugDrawerRoute.route,
                    title = debugDrawerRoute.title,
                    onClick = onClick,
                    content = content,
                )
            }
    }
}
