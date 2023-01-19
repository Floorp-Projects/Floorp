/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.telemetry

import androidx.lifecycle.DefaultLifecycleObserver
import androidx.lifecycle.LifecycleObserver
import androidx.lifecycle.LifecycleOwner
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.support.base.android.Clock
import org.mozilla.fenix.GleanMetrics.EngineTab as EngineMetrics
import org.mozilla.fenix.GleanMetrics.EngineTab.ForegroundMetricsExtra as MetricsExtra

/**
 * [LifecycleObserver] to used on the process lifecycle to measure the amount of tabs getting killed
 * while the app is in the background.
 *
 * See:
 * - https://github.com/mozilla-mobile/android-components/issues/9624
 * - https://github.com/mozilla-mobile/android-components/issues/9997
 */
class TelemetryLifecycleObserver(
    private val store: BrowserStore,
) : DefaultLifecycleObserver {
    private var pausedState: TabState? = null

    override fun onPause(owner: LifecycleOwner) {
        pausedState = createTabState()
    }

    override fun onResume(owner: LifecycleOwner) {
        val lastState = pausedState ?: return
        val currentState = createTabState()

        EngineMetrics.foregroundMetrics.record(
            MetricsExtra(
                backgroundActiveTabs = lastState.activeEngineTabs.toString(),
                backgroundCrashedTabs = lastState.crashedTabs.toString(),
                backgroundTotalTabs = lastState.totalTabs.toString(),
                foregroundActiveTabs = currentState.activeEngineTabs.toString(),
                foregroundCrashedTabs = currentState.crashedTabs.toString(),
                foregroundTotalTabs = currentState.totalTabs.toString(),
                timeInBackground = (currentState.timestamp - lastState.timestamp).toString(),
            ),
        )

        pausedState = null
    }

    private fun createTabState(): TabState {
        val tabsWithEngineSession = store.state.tabs
            .filter { tab -> tab.engineState.engineSession != null }
            .filter { tab -> !tab.engineState.crashed }
            .count()

        val totalTabs = store.state.tabs.count()

        val crashedTabs = store.state.tabs
            .filter { tab -> tab.engineState.crashed }
            .count()

        return TabState(
            activeEngineTabs = tabsWithEngineSession,
            totalTabs = totalTabs,
            crashedTabs = crashedTabs,
        )
    }

    private data class TabState(
        val timestamp: Long = Clock.elapsedRealtime(),
        val totalTabs: Int,
        val crashedTabs: Int,
        val activeEngineTabs: Int,
    )
}
