/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.test.rule

import kotlinx.coroutines.CoroutineDispatcher
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.asCoroutineDispatcher
import kotlinx.coroutines.test.TestDispatcher
import kotlinx.coroutines.test.TestScope
import kotlinx.coroutines.test.UnconfinedTestDispatcher
import kotlinx.coroutines.test.resetMain
import kotlinx.coroutines.test.setMain
import mozilla.components.support.base.utils.NamedThreadFactory
import org.junit.rules.TestWatcher
import org.junit.runner.Description
import java.util.concurrent.Executors

/**
 * Create single threaded dispatcher for test environment.
 */
@Deprecated("Use a `TestDispatcher` from the kotlinx-coroutines-test library", ReplaceWith("UnconfinedTestDispatcher()"))
fun createTestCoroutinesDispatcher(): CoroutineDispatcher = Executors.newSingleThreadExecutor(
    NamedThreadFactory("TestCoroutinesDispatcher"),
).asCoroutineDispatcher()

/**
 * JUnit rule to change Dispatchers.Main in coroutines.
 * This assumes no other calls to `Dispatchers.setMain` are made to override the main dispatcher.
 *
 * @param testDispatcher [TestDispatcher] for handling all coroutines execution.
 * Defaults to [UnconfinedTestDispatcher] which will eagerly enter `launch` or `async` blocks.
 */
@OptIn(ExperimentalCoroutinesApi::class)
class MainCoroutineRule(val testDispatcher: TestDispatcher = UnconfinedTestDispatcher()) : TestWatcher() {
    /**
     * Get a [TestScope] that integrates with `runTest` and can be passed as an argument
     * to the code under test when a [CoroutineScope] is required.
     *
     * This will rely on [testDispatcher] for controlling entering `launch` or `async` blocks.
     */
    val scope by lazy { TestScope(testDispatcher) }

    override fun starting(description: Description) {
        Dispatchers.setMain(testDispatcher)
        super.starting(description)
    }

    override fun finished(description: Description) {
        Dispatchers.resetMain()
        super.finished(description)
    }
}
