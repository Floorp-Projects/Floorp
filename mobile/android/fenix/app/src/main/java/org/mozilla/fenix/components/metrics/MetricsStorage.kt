/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.metrics

import android.content.Context
import kotlinx.coroutines.CoroutineDispatcher
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import mozilla.components.support.utils.ext.getPackageInfoCompat
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.nimbus.FxNimbus
import org.mozilla.fenix.utils.Settings

/**
 * Interface defining functions around persisted local state for certain metrics.
 */
interface MetricsStorage {
    /**
     * Determines whether an [event] should be sent based on locally-stored state.
     */
    suspend fun shouldTrack(event: Event): Boolean

    /**
     * Updates locally-stored state for an [event] that has just been sent.
     */
    suspend fun updateSentState(event: Event)
}

internal class DefaultMetricsStorage(
    context: Context,
    private val settings: Settings,
    private val checkDefaultBrowser: () -> Boolean,
    private val shouldSendGenerally: () -> Boolean = { shouldSendGenerally(context) },
    private val dispatcher: CoroutineDispatcher = Dispatchers.IO,
) : MetricsStorage {
    /**
     * Checks local state to see whether the [event] should be sent.
     */
    override suspend fun shouldTrack(event: Event): Boolean =
        withContext(dispatcher) {
            shouldSendGenerally() && when (event) {
                Event.GrowthData.SetAsDefault -> {
                    !settings.setAsDefaultGrowthSent && checkDefaultBrowser()
                }
                Event.GrowthData.FirstAppOpenForDay -> {
                    settings.resumeGrowthLastSent.hasBeenMoreThanDaySince()
                }
                Event.GrowthData.FirstUriLoadForDay -> {
                    settings.uriLoadGrowthLastSent.hasBeenMoreThanDaySince()
                }
            }
        }

    override suspend fun updateSentState(event: Event) = withContext(dispatcher) {
        when (event) {
            Event.GrowthData.SetAsDefault -> {
                settings.setAsDefaultGrowthSent = true
            }
            Event.GrowthData.FirstAppOpenForDay -> {
                settings.resumeGrowthLastSent = System.currentTimeMillis()
            }
            Event.GrowthData.FirstUriLoadForDay -> {
                settings.uriLoadGrowthLastSent = System.currentTimeMillis()
            }
        }
    }

    private fun Long.hasBeenMoreThanDaySince(): Boolean =
        System.currentTimeMillis() - this > dayMillis

    companion object {
        private const val dayMillis: Long = 1000 * 60 * 60 * 24
        private const val windowStartMillis: Long = dayMillis * 2
        private const val windowEndMillis: Long = dayMillis * 28

        fun shouldSendGenerally(context: Context): Boolean {
            val installedTime = context.packageManager
                .getPackageInfoCompat(context.packageName, 0)
                .firstInstallTime
            val timeDifference = System.currentTimeMillis() - installedTime
            val withinWindow = timeDifference in windowStartMillis..windowEndMillis

            return context.settings().adjustCampaignId.isNotEmpty() &&
                FxNimbus.features.growthData.value().enabled &&
                withinWindow
        }
    }
}
