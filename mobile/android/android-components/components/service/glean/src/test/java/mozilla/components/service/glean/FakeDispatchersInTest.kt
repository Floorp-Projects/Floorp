/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.glean

import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.ObsoleteCoroutinesApi
import org.junit.rules.ExternalResource

/**
 * A class that stubs the coroutine scopes in [Dispatchers] to allow blocking
 * on async API calls when running tests.
 *
 * Note: according to [kotlinx.coroutines.Dispatchers.Unconfined], this will work
 * as long as the API doesn't use any suspending function.
 */
@ExperimentalCoroutinesApi @ObsoleteCoroutinesApi
class FakeDispatchersInTest : ExternalResource() {
    companion object {
        private val testMainScope = CoroutineScope(kotlinx.coroutines.Dispatchers.Unconfined)
        private val originalAPIScope = Dispatchers.API
    }

    override fun before() {
        Dispatchers.API = testMainScope
    }

    override fun after() {
        Dispatchers.API = originalAPIScope
    }
}
