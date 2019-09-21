/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.utils

import kotlinx.coroutines.CoroutineDispatcher
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.asCoroutineDispatcher
import kotlinx.coroutines.test.resetMain
import kotlinx.coroutines.test.setMain
import java.util.concurrent.Executors

/**
 * Create single threaded dispatcher for test environment.
 */
fun createTestCoroutinesDispatcher(): CoroutineDispatcher =
    Executors.newSingleThreadExecutor().asCoroutineDispatcher()

/**
 * Call `@Before` test to setup test coroutines dispatcher as `Main`.
 */
@ExperimentalCoroutinesApi
fun setupTestCoroutinesDispatcher() {
    Dispatchers.setMain(createTestCoroutinesDispatcher())
}

/**
 * Call `@After` test to reset `Main` coroutines dispatcher.
 */
@ExperimentalCoroutinesApi
fun unsetTestCoroutinesDispatcher() {
    Dispatchers.resetMain()
}
