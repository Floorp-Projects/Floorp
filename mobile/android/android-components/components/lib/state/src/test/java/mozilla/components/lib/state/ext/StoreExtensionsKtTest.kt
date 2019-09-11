/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.state.ext

import android.app.Activity
import android.view.View
import android.view.WindowManager
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleOwner
import androidx.lifecycle.LifecycleRegistry
import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.GlobalScope
import kotlinx.coroutines.asCoroutineDispatcher
import kotlinx.coroutines.cancel
import kotlinx.coroutines.cancelAndJoin
import kotlinx.coroutines.channels.consumeEach
import kotlinx.coroutines.flow.collect
import kotlinx.coroutines.launch
import kotlinx.coroutines.runBlocking
import kotlinx.coroutines.test.resetMain
import kotlinx.coroutines.test.setMain
import mozilla.components.lib.state.Store
import mozilla.components.lib.state.TestAction
import mozilla.components.lib.state.TestState
import mozilla.components.lib.state.reducer
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.robolectric.testContext
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.Robolectric
import java.util.concurrent.CountDownLatch
import java.util.concurrent.Executors
import java.util.concurrent.TimeUnit

@RunWith(AndroidJUnit4::class)
class StoreExtensionsKtTest {
    @Before
    @ExperimentalCoroutinesApi
    fun setUp() {
        Dispatchers.setMain(Executors.newSingleThreadExecutor().asCoroutineDispatcher())
    }

    @After
    @ExperimentalCoroutinesApi
    fun tearDown() {
        Dispatchers.resetMain()
    }

    @Test
    fun `Observer will not get registered if lifecycle is already destroyed`() {
        val owner = MockedLifecycleOwner(Lifecycle.State.DESTROYED)

        val store = Store(
            TestState(counter = 23),
            ::reducer
        )

        var stateObserved = false

        store.observe(owner) { stateObserved = true }
        store.dispatch(TestAction.IncrementAction).joinBlocking()

        assertFalse(stateObserved)
    }

    @Test
    fun `Observer will get unregistered if lifecycle gets destroyed`() {
        val owner = MockedLifecycleOwner(Lifecycle.State.STARTED)

        val store = Store(
            TestState(counter = 23),
            ::reducer
        )

        var stateObserved = false
        store.observe(owner) { stateObserved = true }
        assertTrue(stateObserved)

        stateObserved = false
        store.dispatch(TestAction.IncrementAction).joinBlocking()
        assertTrue(stateObserved)

        stateObserved = false
        owner.lifecycleRegistry.currentState = Lifecycle.State.DESTROYED
        store.dispatch(TestAction.IncrementAction).joinBlocking()
        assertFalse(stateObserved)
    }

    @Test
    fun `non-destroy lifecycle changes do not affect observer registration`() {
        val owner = MockedLifecycleOwner(Lifecycle.State.INITIALIZED)

        val store = Store(
            TestState(counter = 23),
            ::reducer
        )

        // Observer does not get invoked since lifecycle is not started
        var stateObserved = false
        store.observe(owner) { stateObserved = true }
        assertFalse(stateObserved)

        // CREATED: Observer does still not get invoked
        stateObserved = false
        owner.lifecycleRegistry.currentState = Lifecycle.State.CREATED
        store.dispatch(TestAction.IncrementAction).joinBlocking()
        assertFalse(stateObserved)

        // STARTED: Observer gets initial state and observers updates
        stateObserved = false
        owner.lifecycleRegistry.currentState = Lifecycle.State.STARTED
        assertTrue(stateObserved)

        stateObserved = false
        store.dispatch(TestAction.IncrementAction).joinBlocking()
        assertTrue(stateObserved)

        // RESUMED: Observer continues to get updates
        stateObserved = false
        owner.lifecycleRegistry.currentState = Lifecycle.State.RESUMED
        store.dispatch(TestAction.IncrementAction).joinBlocking()
        assertTrue(stateObserved)

        // CREATED: Not observing anymore
        stateObserved = false
        owner.lifecycleRegistry.currentState = Lifecycle.State.CREATED
        store.dispatch(TestAction.IncrementAction).joinBlocking()
        assertFalse(stateObserved)

        // DESTROYED: Not observing
        stateObserved = false
        owner.lifecycleRegistry.currentState = Lifecycle.State.DESTROYED
        store.dispatch(TestAction.IncrementAction).joinBlocking()
        assertFalse(stateObserved)
    }

    @Test
    @Synchronized
    @ExperimentalCoroutinesApi // Channel
    fun `Reading state updates from channel`() {
        val owner = MockedLifecycleOwner(Lifecycle.State.INITIALIZED)

        val store = Store(
            TestState(counter = 23),
            ::reducer
        )

        var receivedValue = 0
        var latch = CountDownLatch(1)

        val channel = store.channel(owner)

        val job = GlobalScope.launch {
            channel.consumeEach { state ->
                receivedValue = state.counter
                latch.countDown()
            }
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

        runBlocking { job.cancelAndJoin() }
        assertTrue(channel.isClosedForReceive)

        store.dispatch(TestAction.IncrementAction).joinBlocking()
        assertFalse(latch.await(1, TimeUnit.SECONDS))
        assertEquals(26, receivedValue)
    }

    @Test(expected = IllegalArgumentException::class)
    @ExperimentalCoroutinesApi // Channel
    fun `Creating channel throws if lifecycle is already DESTROYED`() {
        val owner = MockedLifecycleOwner(Lifecycle.State.DESTROYED)

        val store = Store(
            TestState(counter = 23),
            ::reducer
        )

        store.channel(owner)
    }

    @Test
    @Synchronized
    @ExperimentalCoroutinesApi
    fun `Reading state updates from Flow with lifecycle owner`() {
        val owner = MockedLifecycleOwner(Lifecycle.State.INITIALIZED)

        val store = Store(
            TestState(counter = 23),
            ::reducer
        )

        var receivedValue = 0
        var latch = CountDownLatch(1)

        val flow = store.flow(owner)

        val job = GlobalScope.launch {
            flow.collect { state ->
                receivedValue = state.counter
                latch.countDown()
            }
        }

        // Nothing received yet.
        assertFalse(latch.await(1, TimeUnit.SECONDS))
        assertEquals(0, receivedValue)

        // Updating state: Nothing received yet.
        latch = CountDownLatch(1)
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

        runBlocking { job.cancelAndJoin() }

        // Receiving nothing anymore since coroutine is cancelled
        store.dispatch(TestAction.IncrementAction).joinBlocking()
        assertFalse(latch.await(1, TimeUnit.SECONDS))
        assertEquals(26, receivedValue)
    }

    @Test
    @Synchronized
    @ExperimentalCoroutinesApi
    fun `Reading state updates from Flow without lifecycle owner`() {
        val store = Store(
            TestState(counter = 23),
            ::reducer
        )

        var receivedValue = 0
        var latch = CountDownLatch(1)

        val flow = store.flow()

        val job = GlobalScope.launch {
            flow.collect { state ->
                receivedValue = state.counter
                latch.countDown()
            }
        }

        // Receiving immediately
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

        latch = CountDownLatch(1)
        store.dispatch(TestAction.IncrementAction).joinBlocking()
        assertTrue(latch.await(1, TimeUnit.SECONDS))
        assertEquals(26, receivedValue)

        latch = CountDownLatch(1)

        runBlocking { job.cancelAndJoin() }

        // Receiving nothing anymore since coroutine is cancelled
        store.dispatch(TestAction.IncrementAction).joinBlocking()
        assertFalse(latch.await(1, TimeUnit.SECONDS))
        assertEquals(26, receivedValue)
    }

    @Test
    @Synchronized
    @ExperimentalCoroutinesApi
    fun `Reading state from scoped flow without lifecycle owner`() {
        val store = Store(
            TestState(counter = 23),
            ::reducer
        )

        var receivedValue = 0
        var latch = CountDownLatch(1)

        val scope = store.flowScoped() { flow ->
            flow.collect { state ->
                receivedValue = state.counter
                latch.countDown()
            }
        }

        // Receiving immediately
        assertTrue(latch.await(1, TimeUnit.SECONDS))
        assertEquals(23, receivedValue)

        // Updating state: Nothing received yet.
        latch = CountDownLatch(1)
        store.dispatch(TestAction.IncrementAction).joinBlocking()
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

        scope.cancel()

        latch = CountDownLatch(1)
        store.dispatch(TestAction.IncrementAction).joinBlocking()
        assertFalse(latch.await(1, TimeUnit.SECONDS))
        assertEquals(26, receivedValue)
    }

    @Test
    @Synchronized
    @ExperimentalCoroutinesApi
    fun `Reading state from scoped flow with lifecycle owner`() {
        val owner = MockedLifecycleOwner(Lifecycle.State.INITIALIZED)

        val store = Store(
            TestState(counter = 23),
            ::reducer
        )

        var receivedValue = 0
        var latch = CountDownLatch(1)

        val scope = store.flowScoped(owner) { flow ->
            flow.collect { state ->
                receivedValue = state.counter
                latch.countDown()
            }
        }

        // Nothing received yet.
        assertFalse(latch.await(1, TimeUnit.SECONDS))
        assertEquals(0, receivedValue)

        // Updating state: Nothing received yet.
        latch = CountDownLatch(1)
        store.dispatch(TestAction.IncrementAction).joinBlocking()
        assertFalse(latch.await(1, TimeUnit.SECONDS))
        assertEquals(0, receivedValue)

        // Switching to STARTED state: Receiving initial state
        latch = CountDownLatch(1)
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

        scope.cancel()

        latch = CountDownLatch(1)
        store.dispatch(TestAction.IncrementAction).joinBlocking()
        assertFalse(latch.await(1, TimeUnit.SECONDS))
        assertEquals(26, receivedValue)
    }

    @Test
    fun `Observer registered with observeForever will get notified about state changes`() {
        val store = Store(
            TestState(counter = 23),
            ::reducer
        )

        var observedValue = 0

        store.observeForever { state -> observedValue = state.counter }
        assertEquals(23, observedValue)

        store.dispatch(TestAction.IncrementAction).joinBlocking()
        assertEquals(24, observedValue)

        store.dispatch(TestAction.DecrementAction).joinBlocking()
        assertEquals(23, observedValue)
    }

    @Test
    fun `Observer bound to view will get unsubscribed if view gets detached`() {
        val activity = Robolectric.buildActivity(Activity::class.java).create().get()
        val view = View(testContext)
        activity.windowManager.addView(view, WindowManager.LayoutParams(100, 100))

        assertTrue(view.isAttachedToWindow)

        val store = Store(
            TestState(counter = 23),
            ::reducer
        )

        var stateObserved = false
        store.observe(view) { stateObserved = true }
        assertTrue(stateObserved)

        stateObserved = false
        store.dispatch(TestAction.IncrementAction).joinBlocking()
        assertTrue(stateObserved)

        activity.windowManager.removeView(view)
        assertFalse(view.isAttachedToWindow)

        stateObserved = false
        store.dispatch(TestAction.IncrementAction).joinBlocking()
        assertFalse(stateObserved)
    }

    @Test
    fun `Observer bound to view will not get notified about state changes until the view is attached`() {
        val activity = Robolectric.buildActivity(Activity::class.java).create().get()
        val view = View(testContext)

        assertFalse(view.isAttachedToWindow)

        val store = Store(
            TestState(counter = 23),
            ::reducer
        )

        var stateObserved = false
        store.observe(view) { stateObserved = true }
        assertFalse(stateObserved)

        stateObserved = false
        store.dispatch(TestAction.IncrementAction).joinBlocking()
        assertFalse(stateObserved)

        activity.windowManager.addView(view, WindowManager.LayoutParams(100, 100))
        assertTrue(view.isAttachedToWindow)
        assertTrue(stateObserved)

        stateObserved = false
        store.observe(view) { stateObserved = true }
        assertTrue(stateObserved)

        stateObserved = false
        store.observe(view) { stateObserved = true }
        assertTrue(stateObserved)

        activity.windowManager.removeView(view)
        assertFalse(view.isAttachedToWindow)

        stateObserved = false
        store.dispatch(TestAction.IncrementAction).joinBlocking()
        assertFalse(stateObserved)
    }
}

internal class MockedLifecycleOwner(initialState: Lifecycle.State) : LifecycleOwner {
    val lifecycleRegistry = LifecycleRegistry(this).apply {
        currentState = initialState
    }

    override fun getLifecycle(): Lifecycle = lifecycleRegistry
}
