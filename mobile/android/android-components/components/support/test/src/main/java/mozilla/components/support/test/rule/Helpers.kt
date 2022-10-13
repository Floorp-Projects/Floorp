/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:Suppress("INVISIBLE_MEMBER", "INVISIBLE_REFERENCE") // for TestMainDispatcher

package mozilla.components.support.test.rule

import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.TestCoroutineScheduler
import kotlinx.coroutines.test.TestDispatcher
import kotlinx.coroutines.test.TestResult
import kotlinx.coroutines.test.TestScope
import kotlinx.coroutines.test.internal.TestMainDispatcher
import kotlinx.coroutines.test.runTest
import kotlin.reflect.full.companionObject
import kotlin.reflect.full.companionObjectInstance
import kotlin.reflect.full.memberProperties

/**
 * `coroutines.test` default timeout to use when waiting for asynchronous completions of the coroutines
 * managed by a [TestCoroutineScheduler].
 */
private const val DEFAULT_DISPATCH_TIMEOUT_MS = 60_000L

/**
 * Convenience method of executing [testBody] in a new coroutine running with the
 * [TestDispatcher] previously set through [Dispatchers.setMain].
 *
 * Running a test with a shared [TestDispatcher] allows
 * - newly created coroutines inside it will automatically be reparented to the test coroutine context.
 * - leveraging an already set strategy for entering launch / async blocks.
 * - easier scheduling control.
 *
 * @see runTest
 * @see Dispatchers.setMain
 * @see TestDispatcher
 */
@OptIn(ExperimentalCoroutinesApi::class)
fun runTestOnMain(
    dispatchTimeoutMs: Long = DEFAULT_DISPATCH_TIMEOUT_MS,
    testBody: suspend TestScope.() -> Unit,
): TestResult {
    val mainDispatcher = Dispatchers.Main
    require(mainDispatcher is TestMainDispatcher) {
        "A TestDispatcher is not available. Use MainCoroutineRule or Dispatchers.setMain to set one before calling this method"
    }

    // Get the TestDispatcher set through `Dispatchers.setMain(..)`.
    val companionObject = mainDispatcher::class.companionObject
    val companionInstance = mainDispatcher::class.companionObjectInstance
    val testDispatcher = companionObject!!.memberProperties.first().getter.call(companionInstance) as TestDispatcher

    // Delegate to the original implementation of `runTest`. Just with a previously set TestDispatcher.
    runTest(testDispatcher, dispatchTimeoutMs, testBody)
}
