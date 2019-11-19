/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.test.rule

import kotlinx.coroutines.CoroutineDispatcher
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.asCoroutineDispatcher
import kotlinx.coroutines.test.TestCoroutineDispatcher
import kotlinx.coroutines.test.resetMain
import kotlinx.coroutines.test.setMain
import org.junit.rules.TestWatcher
import org.junit.runner.Description
import java.util.concurrent.Executors

/**
 * Create single threaded dispatcher for test environment.
 */
fun createTestCoroutinesDispatcher(): CoroutineDispatcher =
    Executors.newSingleThreadExecutor().asCoroutineDispatcher()

/**
 * JUnit rule to change Dispatchers.Main in coroutines.
 */
@ExperimentalCoroutinesApi
class MainCoroutineRule(val testDispatcher: CoroutineDispatcher = createTestCoroutinesDispatcher()) : TestWatcher() {

    override fun starting(description: Description?) {
        super.starting(description)
        Dispatchers.setMain(testDispatcher)
    }

    override fun finished(description: Description?) {
        super.finished(description)
        Dispatchers.resetMain()

        // If a TestCoroutineDispatcher was used, call its cleanup method
        (testDispatcher as? TestCoroutineDispatcher)?.cleanupTestCoroutines()
    }
}
