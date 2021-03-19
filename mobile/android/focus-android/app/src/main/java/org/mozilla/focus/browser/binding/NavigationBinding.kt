/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.browser.binding

import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.MainScope
import kotlinx.coroutines.cancel
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.collect
import kotlinx.coroutines.flow.filter
import kotlinx.coroutines.flow.filterNotNull
import kotlinx.coroutines.flow.map
import kotlinx.coroutines.launch
import mozilla.components.browser.state.selector.privateTabs
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.lib.state.ext.flow
import mozilla.components.support.base.feature.LifecycleAwareFeature
import mozilla.components.support.ktx.kotlinx.coroutines.flow.ifChanged
import org.mozilla.focus.activity.MainActivity
import org.mozilla.focus.utils.Settings

// This is more or less a translation of the SessionManager.Observer that existed in MainActivity.
// This is still not a great way to deal with navigation. But this was the most minimal change for
// now.
class NavigationBinding(
    private val store: BrowserStore,
    private val activity: MainActivity
) : LifecycleAwareFeature {
    private var shouldShowFirstRun =
        Settings.getInstance(activity).shouldShowFirstrun() && !activity.isCustomTabMode
    private var scope: CoroutineScope? = null

    override fun start() {
        scope = MainScope().also { scope ->
            scope.launch { observeSelectionChanges(store.flow(activity)) }
            scope.launch { observeTabsClosed(store.flow(activity)) }
        }
    }

    override fun stop() {
        scope?.cancel()
    }

    private suspend fun observeSelectionChanges(flow: Flow<BrowserState>) {
        flow.map { state -> state.selectedTabId }
            .ifChanged()
            .filterNotNull()
            .collect { navigateToBrowser() }
    }

    private suspend fun observeTabsClosed(flow: Flow<BrowserState>) {
        flow.map { state -> state.privateTabs.isEmpty() }
            .ifChanged()
            .filter { isEmpty -> isEmpty }
            .collect { navigateToHomeScreen() }
    }

    private fun navigateToHomeScreen() {
        if (activity.isCustomTabMode) {
            return
        }

        if (!activity.isCustomTabMode) {
            activity.showUrlInputScreen()
        }

        if (shouldShowFirstRun) {
            showFirstRun()
        }
    }

    private fun navigateToBrowser() {
        activity.showBrowserScreenForCurrentSession()

        if (shouldShowFirstRun) {
            showFirstRun()
        }
    }

    private fun showFirstRun() {
        shouldShowFirstRun = false
        activity.showFirstrun()
    }
}
