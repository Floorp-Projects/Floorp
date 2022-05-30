/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.telemetry.startuptelemetry

import android.app.Activity
import android.app.Application
import android.os.Bundle
import androidx.annotation.VisibleForTesting
import androidx.annotation.VisibleForTesting.NONE
import androidx.annotation.VisibleForTesting.PRIVATE
import androidx.lifecycle.DefaultLifecycleObserver
import androidx.lifecycle.LifecycleOwner
import androidx.lifecycle.ProcessLifecycleOwner
import mozilla.components.support.base.log.Log
import mozilla.components.support.base.log.logger.Logger

private val logger = Logger("StartupActivityLog")

/**
 * A record of the [Activity] created, started, and stopped events as well as [Application]
 * foreground and background events. See [log] for the log. This class is expected to be
 * registered in [Application.onCreate] by calling [registerInAppOnCreate].
 *
 * To prevent this list from growing infinitely, we clear the list when the application is stopped.
 * This is acceptable from the current requirements: we never need to inspect more than the current
 * start up.
 */
class StartupActivityLog {

    private val _log = mutableListOf<LogEntry>()
    val log: List<LogEntry> = _log

    fun registerInAppOnCreate(
        application: Application,
        processLifecycleOwner: LifecycleOwner = ProcessLifecycleOwner.get()
    ) {
        processLifecycleOwner.lifecycle.addObserver(StartupLogAppLifecycleObserver())
        application.registerActivityLifecycleCallbacks(StartupLogActivityLifecycleCallbacks())
    }

    @VisibleForTesting(otherwise = NONE)
    fun getObserversForTesting() = Pair(StartupLogAppLifecycleObserver(), StartupLogActivityLifecycleCallbacks())

    @VisibleForTesting(otherwise = PRIVATE)
    fun logEntries(loggerArg: Logger = logger, logLevel: Log.Priority = Log.logLevel) {
        // Optimization: we want to avoid the potentially expensive conversions
        // to Strings if we're not going to log anyway.
        if (logLevel > Log.Priority.DEBUG) {
            return
        }

        val transformedEntries = log.map {
            when (it) {
                is LogEntry.AppStarted -> "App-STARTED"
                is LogEntry.AppStopped -> "App-STOPPED"
                is LogEntry.ActivityCreated -> "${it.activityClass.simpleName}-CREATED"
                is LogEntry.ActivityStarted -> "${it.activityClass.simpleName}-STARTED"
                is LogEntry.ActivityStopped -> "${it.activityClass.simpleName}-STOPPED"
            }
        }

        loggerArg.debug(transformedEntries.toString())
    }

    @VisibleForTesting(otherwise = PRIVATE)
    inner class StartupLogAppLifecycleObserver : DefaultLifecycleObserver {
        override fun onStart(owner: LifecycleOwner) {
            _log.add(LogEntry.AppStarted)
        }

        override fun onStop(owner: LifecycleOwner) {
            logEntries()
            _log.clear() // Optimization: see class kdoc for details.
            _log.add(LogEntry.AppStopped)
        }
    }

    @VisibleForTesting(otherwise = PRIVATE)
    inner class StartupLogActivityLifecycleCallbacks : DefaultActivityLifecycleCallbacks {
        override fun onActivityCreated(activity: Activity, bundle: Bundle?) {
            _log.add(LogEntry.ActivityCreated(activity::class.java))
        }

        override fun onActivityStarted(activity: Activity) {
            _log.add(LogEntry.ActivityStarted(activity::class.java))
        }

        override fun onActivityStopped(activity: Activity) {
            _log.add(LogEntry.ActivityStopped(activity::class.java))
        }
    }

    /**
     * A log entry with its detailed information for the [StartupActivityLog].
     */
    sealed class LogEntry {
        object AppStarted : LogEntry()
        object AppStopped : LogEntry()

        data class ActivityCreated(val activityClass: Class<out Activity>) : LogEntry()
        data class ActivityStarted(val activityClass: Class<out Activity>) : LogEntry()
        data class ActivityStopped(val activityClass: Class<out Activity>) : LogEntry()
    }
}
