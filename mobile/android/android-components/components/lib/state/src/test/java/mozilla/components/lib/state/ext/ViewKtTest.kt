/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.state.ext

import android.view.View
import androidx.lifecycle.Lifecycle
import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.ExperimentalCoroutinesApi
import mozilla.components.lib.state.Store
import mozilla.components.lib.state.TestAction
import mozilla.components.lib.state.TestState
import mozilla.components.lib.state.reducer
import mozilla.components.support.test.argumentCaptor
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.mock
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.doNothing
import java.util.concurrent.CountDownLatch
import java.util.concurrent.TimeUnit

@RunWith(AndroidJUnit4::class)
@ExperimentalCoroutinesApi
class ViewKtTest {

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()

    @Test
    @Synchronized
    fun `consumeFrom reads states from store`() {
        val view = mock<View>()
        val owner = MockedLifecycleOwner(Lifecycle.State.INITIALIZED)

        val store = Store(
            TestState(counter = 23),
            ::reducer,
        )

        val onAttachListener = argumentCaptor<View.OnAttachStateChangeListener>()
        var receivedValue = 0
        var latch = CountDownLatch(1)
        doNothing().`when`(view).addOnAttachStateChangeListener(onAttachListener.capture())

        view.consumeFrom(store, owner) { state ->
            receivedValue = state.counter
            latch.countDown()
        }

        // Nothing received yet.
        assertFalse(latch.await(1, TimeUnit.SECONDS))
        assertEquals(0, receivedValue)

        // Updating state: Nothing received yet.
        store.dispatch(TestAction.IncrementAction).joinBlocking()
        assertFalse(latch.await(1, TimeUnit.SECONDS))
        assertEquals(0, receivedValue)

        // Switching to STARTED state: Receiving initial state
        owner.lifecycleRegistry.currentState = Lifecycle.State.STARTED
        assertTrue(latch.await(1, TimeUnit.SECONDS))
        assertEquals(24, receivedValue)
        latch = CountDownLatch(1)

        store.dispatch(TestAction.IncrementAction).joinBlocking()
        assertTrue(latch.await(1, TimeUnit.SECONDS))
        assertEquals(25, receivedValue)
        latch = CountDownLatch(1)

        store.dispatch(TestAction.IncrementAction).joinBlocking()
        assertTrue(latch.await(1, TimeUnit.SECONDS))
        assertEquals(26, receivedValue)
        latch = CountDownLatch(1)

        // View gets detached
        onAttachListener.value.onViewDetachedFromWindow(view)

        store.dispatch(TestAction.IncrementAction).joinBlocking()
        assertFalse(latch.await(1, TimeUnit.SECONDS))
        assertEquals(26, receivedValue)
    }
}
