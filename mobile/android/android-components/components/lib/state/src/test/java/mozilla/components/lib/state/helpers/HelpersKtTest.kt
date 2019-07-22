/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.state.helpers

import mozilla.components.lib.state.Store
import mozilla.components.lib.state.TestAction
import mozilla.components.lib.state.TestState
import mozilla.components.lib.state.reducer
import mozilla.components.support.test.ext.joinBlocking
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class HelpersKtTest {
    @Test
    fun `onlyIfChanged observer only invokes "then" lambda if value has changed`() {
        val store = Store(
            TestState(counter = 23),
            ::reducer
        )

        var callbackExecuted = false
        var callbackValue = false

        store.observeManually(observer = onlyIfChanged<TestState, Boolean>(
            map = { state -> state.counter % 2 == 0 },
            then = { _, _isEven ->
                callbackExecuted = true
                callbackValue = _isEven
            }
        )).also {
            it.resume()
        }

        // Initial notify
        assertTrue(callbackExecuted)
        assertFalse(callbackValue)

        // Mapped value changed
        callbackExecuted = false
        store.dispatch(TestAction.IncrementAction).joinBlocking()
        assertTrue(callbackExecuted)
        assertTrue(callbackValue)

        // Mapped value stays the same
        callbackExecuted = false
        store.dispatch(TestAction.SetValueAction(22)).joinBlocking()
        assertFalse(callbackExecuted)

        // Mapped value changes again
        callbackExecuted = false
        store.dispatch(TestAction.SetValueAction(21)).joinBlocking()
        assertTrue(callbackExecuted)
        assertFalse(callbackValue)
    }

    @Test
    fun `onlyIfChanged observer will not invoke "then" lambda if mapped value is null`() {
        val store = Store(
            TestState(counter = 23),
            ::reducer
        )

        var callbackExecuted = false

        store.observeManually(observer = onlyIfChanged<TestState, TestState>(
            map = { state -> if (state.counter % 2 == 0) null else state },
            then = { _, _ ->
                callbackExecuted = true
            }
        )).also {
            it.resume()
        }

        assertTrue(callbackExecuted)

        callbackExecuted = false
        store.dispatch(TestAction.IncrementAction).joinBlocking()
        assertFalse(callbackExecuted)

        callbackExecuted = false
        store.dispatch(TestAction.IncrementAction).joinBlocking()
        assertTrue(callbackExecuted)

        callbackExecuted = false
        store.dispatch(TestAction.IncrementAction).joinBlocking()
        assertFalse(callbackExecuted)

        callbackExecuted = false
        store.dispatch(TestAction.IncrementAction).joinBlocking()
        assertTrue(callbackExecuted)
    }
}
