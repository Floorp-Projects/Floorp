/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components

import mozilla.components.lib.state.Action
import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.Reducer
import mozilla.components.lib.state.State
import mozilla.components.lib.state.Store
import mozilla.components.support.test.ext.joinBlocking
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

class ChangeDetectionMiddlewareTest {
    @Test
    fun `GIVEN single state property change WHEN action changes that state THEN callback is invoked`() {
        var capturedAction: TestAction? = null
        var preCount = 0
        var postCount = 0
        val middleware: Middleware<TestState, TestAction> = ChangeDetectionMiddleware(
            selector = { it.counter },
            onChange = { action, pre, post ->
                capturedAction = action
                preCount = pre
                postCount = post
            },
        )

        val store = TestStore(
            TestState(counter = preCount, enabled = false),
            ::reducer,
            listOf(middleware),
        )

        store.dispatch(TestAction.IncrementAction).joinBlocking()
        assertTrue(capturedAction is TestAction.IncrementAction)
        assertEquals(0, preCount)
        assertEquals(1, postCount)

        store.dispatch(TestAction.DecrementAction).joinBlocking()
        assertTrue(capturedAction is TestAction.DecrementAction)
        assertEquals(1, preCount)
        assertEquals(0, postCount)
    }

    @Test
    fun `GIVEN multiple state property change WHEN action changes any state THEN callback is invoked`() {
        var capturedAction: TestAction? = null
        var preState = listOf<Any>()
        var postState = listOf<Any>()
        val middleware: Middleware<TestState, TestAction> = ChangeDetectionMiddleware(
            selector = { listOf(it.counter, it.enabled) },
            onChange = { action, pre, post ->
                capturedAction = action
                preState = pre
                postState = post
            },
        )

        val store = TestStore(
            TestState(counter = 0, enabled = false),
            ::reducer,
            listOf(middleware),
        )

        store.dispatch(TestAction.SetEnabled(true)).joinBlocking()
        assertTrue(capturedAction is TestAction.SetEnabled)
        assertEquals(false, preState[1])
        assertEquals(true, postState[1])

        store.dispatch(TestAction.SetEnabled(false)).joinBlocking()
        assertTrue(capturedAction is TestAction.SetEnabled)
        assertEquals(true, preState[1])
        assertEquals(false, postState[1])
    }

    private class TestStore(
        initialState: TestState,
        reducer: Reducer<TestState, TestAction>,
        middleware: List<Middleware<TestState, TestAction>>,
    ) : Store<TestState, TestAction>(initialState, reducer, middleware)

    private data class TestState(
        val counter: Int,
        val enabled: Boolean,
    ) : State

    private sealed class TestAction : Action {
        object IncrementAction : TestAction()
        object DecrementAction : TestAction()
        data class SetEnabled(val enabled: Boolean) : TestAction()
    }

    private fun reducer(state: TestState, action: TestAction): TestState = when (action) {
        is TestAction.IncrementAction -> state.copy(counter = state.counter + 1)
        is TestAction.DecrementAction -> state.copy(counter = state.counter - 1)
        is TestAction.SetEnabled -> state.copy(enabled = action.enabled)
    }
}
