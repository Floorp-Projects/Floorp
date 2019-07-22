/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.state

import mozilla.components.support.test.ext.joinBlocking
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class StoreTest {
    @Test
    fun `Dispatching Action executes reducers and creates new State`() {
        val store = Store(
            TestState(counter = 23),
            ::reducer
        )

        store.dispatch(TestAction.IncrementAction).joinBlocking()

        assertEquals(24, store.state.counter)

        store.dispatch(TestAction.DecrementAction).joinBlocking()
        store.dispatch(TestAction.DecrementAction).joinBlocking()

        assertEquals(22, store.state.counter)
    }

    @Test
    fun `Observer gets notified about state changes`() {
        val store = Store(
            TestState(counter = 23),
            ::reducer
        )

        var observedValue = 0

        store.observeManually { state -> observedValue = state.counter }.also {
            it.resume()
        }

        store.dispatch(TestAction.IncrementAction).joinBlocking()

        assertEquals(24, observedValue)
    }

    @Test
    fun `Observer gets initial value before state changes`() {
        val store = Store(
            TestState(counter = 23),
            ::reducer
        )

        var observedValue = 0

        store.observeManually { state -> observedValue = state.counter }.also {
            it.resume()
        }

        assertEquals(23, observedValue)
    }

    @Test
    fun `Observer does not get notified if state does not change`() {
        val store = Store(
            TestState(counter = 23),
            ::reducer
        )

        var stateChangeObserved = false

        store.observeManually { stateChangeObserved = true }.also {
            it.resume()
        }

        // Initial state observed
        assertTrue(stateChangeObserved)
        stateChangeObserved = false

        store.dispatch(TestAction.DoNothingAction).joinBlocking()

        assertFalse(stateChangeObserved)
    }

    @Test
    fun `Observer does not get notified after unsubscribe`() {
        val store = Store(
            TestState(counter = 23),
            ::reducer
        )

        var observedValue = 0

        val subscription = store.observeManually { state ->
            observedValue = state.counter
        }.also {
            it.resume()
        }

        store.dispatch(TestAction.IncrementAction).joinBlocking()

        assertEquals(24, observedValue)

        store.dispatch(TestAction.DecrementAction).joinBlocking()

        assertEquals(23, observedValue)

        subscription.unsubscribe()

        store.dispatch(TestAction.DecrementAction).joinBlocking()

        assertEquals(23, observedValue)
        assertEquals(22, store.state.counter)
    }
}

fun reducer(state: TestState, action: TestAction): TestState = when (action) {
    is TestAction.IncrementAction -> state.copy(counter = state.counter + 1)
    is TestAction.DecrementAction -> state.copy(counter = state.counter - 1)
    is TestAction.SetValueAction -> state.copy(counter = action.value)
    is TestAction.DoNothingAction -> state
}

data class TestState(
    val counter: Int
) : State

sealed class TestAction : Action {
    object IncrementAction : TestAction()
    object DecrementAction : TestAction()
    object DoNothingAction : TestAction()
    data class SetValueAction(val value: Int) : TestAction()
}
