/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.telemetry.startuptelemetry

import androidx.annotation.VisibleForTesting
import androidx.annotation.VisibleForTesting.NONE
import androidx.annotation.VisibleForTesting.PRIVATE
import androidx.lifecycle.DefaultLifecycleObserver
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleOwner
import mozilla.components.support.base.log.logger.Logger
import org.mozilla.focus.GleanMetrics.PerfStartup
import org.mozilla.focus.activity.MainActivity
import org.mozilla.focus.telemetry.startuptelemetry.StartupPathProvider.StartupPath
import org.mozilla.focus.telemetry.startuptelemetry.StartupStateProvider.StartupState

private val activityClass = MainActivity::class.java

private val logger = Logger("StartupTypeTelemetry")

/**
 * Records telemetry for the number of start ups. See the
 * [Fenix perf glossary](https://wiki.mozilla.org/index.php?title=Performance/Fenix/Glossary)
 * for specific definitions.
 *
 * This should be a member variable of [MainActivity] because its data is tied to the lifecycle of an
 * Activity. Call [attachOnMainActivityOnCreate] for this class to work correctly.
 *
 * N.B.: this class is lightly hardcoded to MainActivity.
 */
class StartupTypeTelemetry(
    private val startupStateProvider: StartupStateProvider,
    private val startupPathProvider: StartupPathProvider
) {

    fun attachOnMainActivityOnCreate(lifecycle: Lifecycle) {
        lifecycle.addObserver(StartupTypeLifecycleObserver())
    }

    private fun getTelemetryLabel(startupState: StartupState, startupPath: StartupPath): String {
        // We don't use the enum name directly to avoid unintentional changes when refactoring.
        val stateLabel = when (startupState) {
            StartupState.COLD -> "cold"
            StartupState.WARM -> "warm"
            StartupState.HOT -> "hot"
            StartupState.UNKNOWN -> "unknown"
        }

        val pathLabel = when (startupPath) {
            StartupPath.MAIN -> "main"
            StartupPath.VIEW -> "view"

            // To avoid combinatorial explosion in label names, we bucket NOT_SET into UNKNOWN.
            StartupPath.NOT_SET,
            StartupPath.UNKNOWN -> "unknown"
        }

        return "${stateLabel}_$pathLabel"
    }

    @VisibleForTesting(otherwise = NONE)
    fun getTestCallbacks() = StartupTypeLifecycleObserver()

    @VisibleForTesting(otherwise = PRIVATE)
    fun record() {
        val startupState = startupStateProvider.getStartupStateForStartedActivity(activityClass)
        val startupPath = startupPathProvider.startupPathForActivity
        val label = getTelemetryLabel(startupState, startupPath)

        PerfStartup.startupType[label].add(1)
        logger.info("Recorded start up: $label")
    }

    @VisibleForTesting(otherwise = PRIVATE)
    inner class StartupTypeLifecycleObserver : DefaultLifecycleObserver {
        private var shouldRecordStart = false

        override fun onStart(owner: LifecycleOwner) {
            shouldRecordStart = true
        }

        override fun onResume(owner: LifecycleOwner) {
            // We must record in onResume because the StartupStateProvider can only be called for
            // STARTED activities and we can't guarantee our onStart is called before its.
            //
            // We only record if start was called for this resume to avoid recording
            // for onPause -> onResume states.
            if (shouldRecordStart) {
                record()
                shouldRecordStart = false
            }
        }
    }
}
