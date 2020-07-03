/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.state.ext

import android.view.View
import androidx.fragment.app.Fragment
import androidx.lifecycle.Lifecycle
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
import org.mockito.Mockito.doNothing
import org.mockito.Mockito.doReturn
import java.util.concurrent.CountDownLatch
import java.util.concurrent.TimeUnit

@ExperimentalCoroutinesApi
class FragmentKtTest {

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()

    @Test
    @Synchronized
    fun `consumeFrom reads states from store`() {
        val fragment = mock<Fragment>()
        val view = mock<View>()
        val owner = MockedLifecycleOwner(Lifecycle.State.INITIALIZED)

        val store = Store(
            TestState(counter = 23),
            ::reducer
        )

        val onAttachListener = argumentCaptor<View.OnAttachStateChangeListener>()
        var receivedValue = 0
        var latch = CountDownLatch(1)

        doNothing().`when`(view).addOnAttachStateChangeListener(onAttachListener.capture())
        doReturn(true).`when`(fragment).isAdded
        doReturn(view).`when`(fragment).view
        doReturn(owner.lifecycle).`when`(fragment).lifecycle

        fragment.consumeFrom(store) { state ->
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

    @Test
    @Synchronized
    fun `consumeFrom does not run when fragment is not added`() {
        val fragment = mock<Fragment>()
        val view = mock<View>()
        val owner = MockedLifecycleOwner(Lifecycle.State.STARTED)

        val store = Store(
                TestState(counter = 23),
                ::reducer
        )

        var receivedValue = 0
        var latch = CountDownLatch(1)

        doReturn(view).`when`(fragment).view
        doReturn(owner.lifecycle).`when`(fragment).lifecycle

        doReturn(true).`when`(fragment).isAdded

        fragment.consumeFrom(store) { state ->
            receivedValue = state.counter
            latch.countDown()
        }

        assertTrue(latch.await(1, TimeUnit.SECONDS))
        assertEquals(23, receivedValue)

        latch = CountDownLatch(1)
        store.dispatch(TestAction.IncrementAction).joinBlocking()
        assertTrue(latch.await(1, TimeUnit.SECONDS))
        assertEquals(24, receivedValue)

        latch = CountDownLatch(1)
        store.dispatch(TestAction.IncrementAction).joinBlocking()
        assertTrue(latch.await(1, TimeUnit.SECONDS))
        assertEquals(25, receivedValue)

        doReturn(false).`when`(fragment).isAdded

        latch = CountDownLatch(1)
        store.dispatch(TestAction.IncrementAction).joinBlocking()
        assertFalse(latch.await(1, TimeUnit.SECONDS))
        assertEquals(25, receivedValue)

        latch = CountDownLatch(1)
        store.dispatch(TestAction.IncrementAction).joinBlocking()
        assertFalse(latch.await(1, TimeUnit.SECONDS))
        assertEquals(25, receivedValue)

        doReturn(true).`when`(fragment).isAdded

        latch = CountDownLatch(1)
        store.dispatch(TestAction.IncrementAction).joinBlocking()
        assertTrue(latch.await(1, TimeUnit.SECONDS))
        assertEquals(28, receivedValue)
    }
}
