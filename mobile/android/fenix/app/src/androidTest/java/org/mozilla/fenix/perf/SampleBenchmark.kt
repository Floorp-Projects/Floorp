/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.perf

import androidx.benchmark.junit4.BenchmarkRule
import androidx.benchmark.junit4.measureRepeated
import androidx.test.ext.junit.runners.AndroidJUnit4
import org.junit.Ignore
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith

/**
 * To run this benchmark:
 * - Comment out @Ignore: DO NOT COMMIT THIS!
 * - See run instructions in app/benchmark.gradle
 *
 * See https://developer.android.com/studio/profile/benchmark#write-benchmark for how to write a
 * real benchmark, including testing UI code. See
 * https://developer.android.com/studio/profile/benchmark#what-to-benchmark for when jetpack
 * microbenchmark is a good fit.
 */
@Ignore("This is a sample: we don't want it to run when we run all the tests")
@RunWith(AndroidJUnit4::class)
class SampleBenchmark {
    @get:Rule
    val benchmarkRule = BenchmarkRule()

    @Test
    fun additionBenchmark() = benchmarkRule.measureRepeated {
        var i = 0
        while (i < 10_000_000) {
            i++
        }
    }
}
