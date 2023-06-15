/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.navigation

import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.cancel
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.distinctUntilChangedBy
import kotlinx.coroutines.flow.map
import mozilla.components.lib.state.ext.flowScoped
import mozilla.components.support.base.feature.LifecycleAwareFeature
import org.mozilla.focus.state.AppState
import org.mozilla.focus.state.AppStore
import org.mozilla.focus.state.Screen

/**
 * The [Navigator] subscribes to the [AppStore] and initiates a navigation with the help of the
 * provided [MainActivityNavigation] if the [Screen] in the [AppState] changes.
 */
class Navigator(
    val store: AppStore,
    val navigation: MainActivityNavigation,
) : LifecycleAwareFeature {
    private var scope: CoroutineScope? = null

    override fun start() {
        scope = store.flowScoped { flow -> subscribe(flow) }
    }

    override fun stop() {
        scope?.cancel()
    }

    private suspend fun subscribe(flow: Flow<AppState>) {
        flow.map { state -> state.screen }
            .distinctUntilChangedBy { screen -> screen.id }
            .collect { screen -> navigateTo(screen) }
    }

    private fun navigateTo(screen: Screen) {
        when (screen) {
            is Screen.Home -> navigation.home()
            is Screen.Browser -> navigation.browser(screen.tabId)
            is Screen.EditUrl -> navigation.edit(
                screen.tabId,
            )
            is Screen.FirstRun -> navigation.firstRun()
            is Screen.Locked -> navigation.lock(screen.bundle)
            is Screen.Settings -> navigation.settings(screen.page)
            is Screen.SitePermissionOptionsScreen -> navigation.sitePermissionOptionsFragment(screen.sitePermission)
            is Screen.OnboardingSecondScreen -> navigation.showOnBoardingSecondScreen()
        }
    }
}
