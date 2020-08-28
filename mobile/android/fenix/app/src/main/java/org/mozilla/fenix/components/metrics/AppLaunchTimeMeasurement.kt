/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.metrics

import android.os.Process
import android.os.SystemClock
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import org.mozilla.fenix.components.metrics.Event.AppAllStartup.Type
import org.mozilla.fenix.components.metrics.Event.AppAllStartup.Type.ERROR
import org.mozilla.fenix.components.metrics.Event.AppAllStartup.Type.WARM
import org.mozilla.fenix.components.metrics.Event.AppAllStartup.Type.HOT
import org.mozilla.fenix.components.metrics.Event.AppAllStartup.Type.COLD
import org.mozilla.fenix.perf.Stat

/**
 * Handles the logic of figuring out launch time for cold, warm and hot startup.
 */
class AppLaunchTimeMeasurement(private val stats: Stat = Stat()) {

    private var isOnPreDrawCalled = false

    private var applicationOnCreateTimeStampNanoSeconds: Long? = null
    private var homeActivityInitTimeStampNanoSeconds: Long? = null
    private var homeActivityOnRestartTimeStampNanoSeconds: Long? = null
    // we are considering screen to be visible after the first pre draw call.
    private var homeActivityOnPreDrawTimeStampNanoSeconds: Long? = null

    fun onHomeActivityOnCreate(activityInitNanos: Long) {
        this.homeActivityInitTimeStampNanoSeconds = activityInitNanos
    }

    fun onHomeActivityOnRestart(activityOnRestartNanos: Long = SystemClock.elapsedRealtimeNanos()) {
        homeActivityOnRestartTimeStampNanoSeconds = activityOnRestartNanos
    }

    fun onFirstFramePreDraw(activityOnPreDrawNanos: Long = SystemClock.elapsedRealtimeNanos()) {
        isOnPreDrawCalled = true
        homeActivityOnPreDrawTimeStampNanoSeconds = activityOnPreDrawNanos
    }

    /**
     * if we have both start and finish time for launch, return the difference otherwise return null.
     */
    suspend fun getApplicationLaunchTime(startupType: Type): Long? = withContext(Dispatchers.IO) {
        when {
            // one use case is user launching the app and quicky pressing back button. in that case
            // there will be no onPredraw call but activity will call onStop().
            !isOnPreDrawCalled -> {
                null
            }
            else -> {
                when (startupType) {
                    COLD -> {
                        applicationOnCreateTimeStampNanoSeconds =
                            stats.getProcessStartTimeStampNano(Process.myPid())
                        homeActivityOnPreDrawTimeStampNanoSeconds!!.minus(
                            applicationOnCreateTimeStampNanoSeconds!!
                        )
                    }
                    WARM -> {
                        homeActivityOnPreDrawTimeStampNanoSeconds!!.minus(
                            homeActivityInitTimeStampNanoSeconds!!
                        )
                    }
                    HOT -> {
                        homeActivityOnPreDrawTimeStampNanoSeconds!!.minus(
                            homeActivityOnRestartTimeStampNanoSeconds!!
                        )
                    }
                    ERROR -> {
                        null
                    }
                }
            }
        }
    }
}
