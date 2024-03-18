/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.benchmark.utils

import androidx.annotation.IntRange
import androidx.benchmark.macro.CompilationMode
import androidx.benchmark.macro.MacrobenchmarkScope
import androidx.benchmark.macro.Metric
import androidx.benchmark.macro.StartupMode
import androidx.benchmark.macro.junit4.MacrobenchmarkRule

/**
 * Extension function that calls [MacrobenchmarkRule.measureRepeated] with
 * defaults parameters set for [packageName] and [iterations].
 */
fun MacrobenchmarkRule.measureRepeatedDefault(
    packageName: String = TARGET_PACKAGE,
    metrics: List<Metric>,
    compilationMode: CompilationMode = CompilationMode.DEFAULT,
    startupMode: StartupMode? = null,
    @IntRange(from = 1)
    iterations: Int = DEFAULT_ITERATIONS,
    setupBlock: MacrobenchmarkScope.() -> Unit = {},
    measureBlock: MacrobenchmarkScope.() -> Unit,
) {
    measureRepeated(
        packageName = packageName,
        metrics = metrics,
        compilationMode = compilationMode,
        startupMode = startupMode,
        iterations = iterations,
        setupBlock = setupBlock,
        measureBlock = measureBlock,
    )
}
