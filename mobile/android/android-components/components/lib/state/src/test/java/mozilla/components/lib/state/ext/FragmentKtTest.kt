/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.state.ext

import android.app.Activity
import android.view.View
import android.view.WindowManager
import androidx.fragment.app.Fragment
import androidx.lifecycle.Lifecycle
import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.ObsoleteCoroutinesApi
import kotlinx.coroutines.asCoroutineDispatcher
import kotlinx.coroutines.test.resetMain
import kotlinx.coroutines.test.setMain
import mozilla.components.lib.state.Store
import mozilla.components.lib.state.TestAction
import mozilla.components.lib.state.TestState
import mozilla.components.lib.state.reducer
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.doReturn
import org.robolectric.Robolectric
import java.util.concurrent.CountDownLatch
import java.util.concurrent.Executors
import java.util.concurrent.TimeUnit

@RunWith(AndroidJUnit4::class)
class FragmentKtTest {
    @Before
    @ExperimentalCoroutinesApi
    fun setUp() {
        // We create a separate thread for the main dispatcher so that we do not deadlock our test
        // thread.
        Dispatchers.setMain(Executors.newSingleThreadExecutor().asCoroutineDispatcher())
    }

    @After
    @ExperimentalCoroutinesApi
    fun tearDown() {
        Dispatchers.resetMain()
    }

    @Test
    @Synchronized
    @ExperimentalCoroutinesApi // consumeFrom
    @ObsoleteCoroutinesApi // consumeFrom
    fun `consumeFrom reads states from store`() {
        val owner = MockedLifecycleOwner(Lifecycle.State.INITIALIZED)

        val store = Store(
            TestState(counter = 23),
            ::reducer
        )

        val fragment: Fragment = mock()
        doReturn(true).`when`(fragment).isAdded

        val view = View(testContext)
        val activity = Robolectric.buildActivity(Activity::class.java).create().get()
        activity.windowManager.addView(view, WindowManager.LayoutParams(100, 100))
        assertTrue(view.isAttachedToWindow)
        doReturn(view).`when`(fragment).view
        doReturn(owner.lifecycle).`when`(fragment).lifecycle

        var receivedValue = 0
        var latch = CountDownLatch(1)

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
        owner.lifecycleRegistry.markState(Lifecycle.State.STARTED)
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
        activity.windowManager.removeView(view)
        assertFalse(view.isAttachedToWindow)

        store.dispatch(TestAction.IncrementAction).joinBlocking()
        assertFalse(latch.await(1, TimeUnit.SECONDS))
        assertEquals(26, receivedValue)
    }

    @Test
    @Synchronized
    @ExperimentalCoroutinesApi // consumeFrom
    @ObsoleteCoroutinesApi // consumeFrom
    fun `consumeFrom does not run when fragment is not added`() {
        val owner = MockedLifecycleOwner(Lifecycle.State.STARTED)

        val store = Store(
                TestState(counter = 23),
                ::reducer
        )

        val fragment: Fragment = mock()

        val view = View(testContext)
        val activity = Robolectric.buildActivity(Activity::class.java).create().get()
        activity.windowManager.addView(view, WindowManager.LayoutParams(100, 100))
        assertTrue(view.isAttachedToWindow)
        doReturn(view).`when`(fragment).view
        doReturn(owner.lifecycle).`when`(fragment).lifecycle

        doReturn(true).`when`(fragment).isAdded

        var receivedValue = 0
        var latch = CountDownLatch(1)

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
