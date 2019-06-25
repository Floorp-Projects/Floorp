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
import mozilla.components.lib.state.Store
import mozilla.components.lib.state.TestAction
import mozilla.components.lib.state.TestState
import mozilla.components.lib.state.reducer
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.Robolectric

@RunWith(AndroidJUnit4::class)
class StoreExtensionsKtTest {
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
        owner.lifecycleRegistry.markState(Lifecycle.State.DESTROYED)
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

        var stateObserved = false
        store.observe(owner) { stateObserved = true }
        assertTrue(stateObserved)

        // CREATED
        stateObserved = false
        owner.lifecycleRegistry.markState(Lifecycle.State.CREATED)
        store.dispatch(TestAction.IncrementAction).joinBlocking()
        assertTrue(stateObserved)

        // STARTED
        stateObserved = false
        owner.lifecycleRegistry.markState(Lifecycle.State.STARTED)
        store.dispatch(TestAction.IncrementAction).joinBlocking()
        assertTrue(stateObserved)

        // RESUMED
        stateObserved = false
        owner.lifecycleRegistry.markState(Lifecycle.State.RESUMED)
        store.dispatch(TestAction.IncrementAction).joinBlocking()
        assertTrue(stateObserved)

        // CREATED
        stateObserved = false
        owner.lifecycleRegistry.markState(Lifecycle.State.CREATED)
        store.dispatch(TestAction.IncrementAction).joinBlocking()
        assertTrue(stateObserved)

        // DESTROYED
        stateObserved = false
        owner.lifecycleRegistry.markState(Lifecycle.State.DESTROYED)
        store.dispatch(TestAction.IncrementAction).joinBlocking()
        assertFalse(stateObserved)
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
}

private class MockedLifecycleOwner(initialState: Lifecycle.State) : LifecycleOwner {
    val lifecycleRegistry = LifecycleRegistry(this).apply {
        markState(initialState)
    }

    override fun getLifecycle(): Lifecycle = lifecycleRegistry
}
