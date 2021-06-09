/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.telemetry

import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleObserver
import androidx.lifecycle.OnLifecycleEvent
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.support.base.android.Clock
import org.mozilla.fenix.GleanMetrics.EngineTab as EngineMetrics
import org.mozilla.fenix.GleanMetrics.EngineTab.foregroundMetricsKeys as MetricsKeys

/**
 * [LifecycleObserver] to used on the process lifecycle to measure the amount of tabs getting killed
 * while the app is in the background.
 *
 * See:
 * - https://github.com/mozilla-mobile/android-components/issues/9624
 * - https://github.com/mozilla-mobile/android-components/issues/9997
 */
class TelemetryLifecycleObserver(
    private val store: BrowserStore
) : LifecycleObserver {
    private var pausedState: TabState? = null

    @OnLifecycleEvent(Lifecycle.Event.ON_PAUSE)
    fun onPause() {
        pausedState = createTabState()
    }

    @OnLifecycleEvent(Lifecycle.Event.ON_RESUME)
    fun onResume() {
        val lastState = pausedState ?: return
        val currentState = createTabState()

        @Suppress("DEPRECATION")
        // FIXME(#19967): Migrate to non-deprecated API.
        EngineMetrics.foregroundMetrics.record(mapOf(
            MetricsKeys.backgroundActiveTabs to lastState.activeEngineTabs.toString(),
            MetricsKeys.backgroundCrashedTabs to lastState.crashedTabs.toString(),
            MetricsKeys.backgroundTotalTabs to lastState.totalTabs.toString(),
            MetricsKeys.foregroundActiveTabs to currentState.activeEngineTabs.toString(),
            MetricsKeys.foregroundCrashedTabs to currentState.crashedTabs.toString(),
            MetricsKeys.foregroundTotalTabs to currentState.totalTabs.toString(),
            MetricsKeys.timeInBackground to (currentState.timestamp - lastState.timestamp).toString()
        ))

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
            crashedTabs = crashedTabs
        )
    }
}

private data class TabState(
    val timestamp: Long = Clock.elapsedRealtime(),
    val totalTabs: Int,
    val crashedTabs: Int,
    val activeEngineTabs: Int
)
