/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.state.ext

import androidx.compose.ui.test.junit4.createComposeRule
import kotlinx.coroutines.runBlocking
import mozilla.components.lib.state.Action
import mozilla.components.lib.state.State
import mozilla.components.lib.state.Store
import org.junit.Assert.assertEquals
import org.junit.Rule
import org.junit.Test

class ComposeExtensionsKtTest {
    @get:Rule
    val rule = createComposeRule()

    @Test
    fun usingInitialValue() {
        val store = Store(
            initialState = TestState(counter = 42),
            reducer = ::reducer,
        )

        var value: Int? = null

        rule.setContent {
            val composeState = store.observeAsComposableState { state -> state.counter * 2 }
            value = composeState.value
        }

        assertEquals(84, value)
    }

    @Test
    fun receivingUpdates() {
        val store = Store(
            initialState = TestState(counter = 42),
            reducer = ::reducer,
        )

        var value: Int? = null

        rule.setContent {
            val composeState = store.observeAsComposableState { state -> state.counter * 2 }
            value = composeState.value
        }

        store.dispatchBlockingOnIdle(TestAction.IncrementAction)

        rule.runOnIdle {
            assertEquals(86, value)
        }
    }

    @Test
    fun usingInitialValueWithUpdates() {
        val loading = "Loading"
        val content = "Content"
        val store = Store(
            initialState = TestState(counter = 0),
            reducer = ::reducer,
        )

        val value = mutableListOf<String>()

        rule.setContent {
            val composeState = store.observeAsState(
                initialValue = loading,
                map = { if (it.counter < 5) loading else content },
            )
            value.add(composeState.value)
        }

        rule.runOnIdle {
            // Initial value when counter is 0.
            assertEquals(listOf("Loading"), value)
        }

        store.dispatchBlockingOnIdle(TestAction.IncrementAction)
        store.dispatchBlockingOnIdle(TestAction.IncrementAction)
        store.dispatchBlockingOnIdle(TestAction.IncrementAction)
        store.dispatchBlockingOnIdle(TestAction.IncrementAction)

        rule.runOnIdle {
            // Value after 4 increments, aka counter is 4. Note that it doesn't recompose here
            // as the mapped value has stayed the same. We have 1 item in the list and not 5.
            assertEquals(listOf(loading), value)
        }

        // 5th increment
        store.dispatchBlockingOnIdle(TestAction.IncrementAction)

        rule.runOnIdle {
            assertEquals(listOf(loading, content), value)
            assertEquals(content, value.last())
        }
    }

    @Test
    fun receivingUpdatesForPartialStateUpdateOnly() {
        val store = Store(
            initialState = TestState(counter = 42),
            reducer = ::reducer,
        )

        var value: Int? = null

        rule.setContent {
            val composeState = store.observeAsComposableState(
                map = { state -> state.counter * 2 },
                observe = { state -> state.text },
            )
            value = composeState.value
        }

        assertEquals(84, value)

        store.dispatchBlockingOnIdle(TestAction.IncrementAction)

        rule.runOnIdle {
            // State value didn't change because value returned by `observer` function did not change
            assertEquals(84, value)
        }

        store.dispatchBlockingOnIdle(TestAction.SetTextAction("Hello World"))

        rule.runOnIdle {
            // Now, after the value from the observer function changed, we are seeing the new value
            assertEquals(86, value)
        }

        store.dispatchBlockingOnIdle(TestAction.SetValueAction(23))

        rule.runOnIdle {
            // Observer function result is the same, no state update
            assertEquals(86, value)
        }

        store.dispatchBlockingOnIdle(TestAction.SetTextAction("Hello World"))

        rule.runOnIdle {
            // Text was updated to the same value, observer function result is the same, no state update
            assertEquals(86, value)
        }

        store.dispatchBlockingOnIdle(TestAction.SetTextAction("Hello World Again"))

        rule.runOnIdle {
            // Now, after the value from the observer function changed, we are seeing the new value
            assertEquals(46, value)
        }
    }

    private fun Store<TestState, TestAction>.dispatchBlockingOnIdle(action: TestAction) {
        rule.runOnIdle {
            val job = dispatch(action)
            runBlocking { job.join() }
        }
    }
}

fun reducer(state: TestState, action: TestAction): TestState = when (action) {
    is TestAction.IncrementAction -> state.copy(counter = state.counter + 1)
    is TestAction.DecrementAction -> state.copy(counter = state.counter - 1)
    is TestAction.SetValueAction -> state.copy(counter = action.value)
    is TestAction.SetTextAction -> state.copy(text = action.text)
}

data class TestState(
    val counter: Int,
    val text: String = "",
) : State

sealed class TestAction : Action {
    object IncrementAction : TestAction()
    object DecrementAction : TestAction()
    data class SetValueAction(val value: Int) : TestAction()
    data class SetTextAction(val text: String) : TestAction()
}
