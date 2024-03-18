/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.state.helpers

import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.flow.Flow
import mozilla.components.lib.state.Store
import mozilla.components.lib.state.TestAction
import mozilla.components.lib.state.TestState
import mozilla.components.lib.state.reducer
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Assert.fail
import org.junit.Rule
import org.junit.Test

@ExperimentalCoroutinesApi
class AbstractBindingTest {

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()

    @Test
    fun `binding onState is invoked when a flow is created`() {
        val store = Store(
            TestState(counter = 0),
            ::reducer,
        )

        val binding = TestBinding(store)

        assertFalse(binding.invoked)

        binding.start()

        assertTrue(binding.invoked)
    }

    @Test
    fun `binding has no state changes when only stop is invoked`() {
        val store = Store(
            TestState(counter = 0),
            ::reducer,
        )

        val binding = TestBinding(store)

        assertFalse(binding.invoked)

        binding.stop()

        assertFalse(binding.invoked)
    }

    @Test
    fun `binding does not get state updates after stopped`() {
        val store = Store(
            TestState(counter = 0),
            ::reducer,
        )

        var counter = 0

        val binding = TestBinding(store) {
            counter++
            // After we stop, we shouldn't get updates for the third action dispatched.
            if (counter >= 3) {
                fail()
            }
        }

        store.dispatch(TestAction.IncrementAction).joinBlocking()

        binding.start()

        store.dispatch(TestAction.IncrementAction).joinBlocking()

        binding.stop()

        store.dispatch(TestAction.IncrementAction).joinBlocking()
    }
}

@ExperimentalCoroutinesApi
class TestBinding(
    store: Store<TestState, TestAction>,
    private val onStateUpdated: (TestState) -> Unit = {},
) : AbstractBinding<TestState>(store) {
    var invoked = false
    override suspend fun onState(flow: Flow<TestState>) {
        invoked = true
        flow.collect { onStateUpdated(it) }
    }
}
