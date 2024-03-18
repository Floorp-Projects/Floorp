/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.test.libstate.ext

import mozilla.components.lib.state.Action
import mozilla.components.lib.state.State
import mozilla.components.lib.state.Store
import org.junit.Assert.assertEquals
import org.junit.Test

class StoreTest {

    @Test
    fun `waitUntilIdle blocks and returns once reducers were executed`() {
        val store = Store(
            TestState(counter = 23),
            ::reducer,
        )

        store.dispatch(TestAction.IncrementAction)
        store.waitUntilIdle()
        assertEquals(24, store.state.counter)

        store.dispatch(TestAction.DecrementAction)
        store.dispatch(TestAction.DecrementAction)
        store.waitUntilIdle()
        assertEquals(22, store.state.counter)
    }
}

fun reducer(state: TestState, action: TestAction): TestState = when (action) {
    is TestAction.IncrementAction -> state.copy(counter = state.counter + 1)
    is TestAction.DecrementAction -> state.copy(counter = state.counter - 1)
}

data class TestState(val counter: Int) : State

sealed class TestAction : Action {
    object IncrementAction : TestAction()
    object DecrementAction : TestAction()
}
