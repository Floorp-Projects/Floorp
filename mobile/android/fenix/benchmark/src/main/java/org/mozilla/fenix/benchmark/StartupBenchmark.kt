/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.benchmark

import androidx.benchmark.macro.StartupMode
import androidx.benchmark.macro.StartupTimingMetric
import androidx.benchmark.macro.junit4.MacrobenchmarkRule
import androidx.test.ext.junit.runners.AndroidJUnit4
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.benchmark.utils.measureRepeatedDefault

/**
 * This is a startup benchmark.
 * It navigates to the device's home screen, and launches the default activity.
 *
 * Before running this benchmark,
 * switch your app's active build variant in the Studio (affects Studio runs only)
 *
 * Run this benchmark from Studio to see startup measurements, and captured system traces
 * for investigating your app's performance.
 */
@RunWith(AndroidJUnit4::class)
class StartupBenchmark {
    @get:Rule
    val benchmarkRule = MacrobenchmarkRule()

    @Test
    fun startupCold() = startupBenchmark(StartupMode.COLD)

    @Test
    fun startupWarm() = startupBenchmark(StartupMode.WARM)

    @Test
    fun startupHot() = startupBenchmark(StartupMode.HOT)

    private fun startupBenchmark(startupMode: StartupMode) = benchmarkRule.measureRepeatedDefault(
        metrics = listOf(StartupTimingMetric()),
        startupMode = startupMode,
        setupBlock = {
            pressHome()
        }
    ) {
        startActivityAndWait()
    }
}
