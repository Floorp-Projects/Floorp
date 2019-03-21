/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, you can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.base.observer

import android.app.Activity
import android.arch.lifecycle.GenericLifecycleObserver
import android.arch.lifecycle.Lifecycle
import android.arch.lifecycle.LifecycleObserver
import android.arch.lifecycle.LifecycleOwner
import android.content.Context
import android.view.View
import android.view.WindowManager
import androidx.test.core.app.ApplicationProvider
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.mock
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify
import org.robolectric.Robolectric
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class ObserverRegistryTest {
    private val context: Context
        get() = ApplicationProvider.getApplicationContext()

    @Test
    fun `registered observer gets notified`() {
        val registry = ObserverRegistry<TestObserver>()

        val observer = TestObserver()
        registry.register(observer)

        assertFalse(observer.notified)

        registry.notifyObservers {
            somethingChanged()
        }

        assertTrue(observer.notified)
    }

    @Test
    fun `observer does not get notified again after unregistering`() {
        val registry = ObserverRegistry<TestObserver>()

        val observer = TestObserver()
        registry.register(observer)

        assertFalse(observer.notified)

        registry.notifyObservers {
            somethingChanged()
        }

        assertTrue(observer.notified)

        observer.notified = false
        registry.unregister(observer)

        assertFalse(observer.notified)

        registry.notifyObservers {
            somethingChanged()
        }

        assertFalse(observer.notified)
    }

    @Test
    fun `observer gets notified multiple times`() {
        val registry = ObserverRegistry<TestObserver>()

        val observer = TestObserver()
        registry.register(observer)

        assertFalse(observer.notified)

        registry.notifyObservers {
            somethingChanged()
        }

        assertTrue(observer.notified)
        observer.notified = false

        registry.notifyObservers {
            somethingChanged()
        }

        assertTrue(observer.notified)
    }

    @Test
    fun `observer does not get notified when unregistered immediately`() {
        val registry = ObserverRegistry<TestObserver>()

        val observer = TestObserver()

        registry.register(observer)
        registry.unregister(observer)

        assertFalse(observer.notified)

        registry.notifyObservers {
            somethingChanged()
        }

        assertFalse(observer.notified)
    }

    @Test
    fun `observer will not get registered if lifecycle state is DESTROYED`() {
        val owner = MockedLifecycleOwner(MockedLifecycle(Lifecycle.State.DESTROYED))

        val registry = ObserverRegistry<TestObserver>()
        val observer = TestObserver()

        registry.register(observer, owner)

        assertFalse(observer.notified)

        registry.notifyObservers {
            somethingChanged()
        }

        assertFalse(observer.notified)
    }

    @Test
    fun `observer will get removed if lifecycle gets destroyed`() {
        val mockedLifecycle = MockedLifecycle(Lifecycle.State.STARTED)
        val owner = MockedLifecycleOwner(mockedLifecycle)

        val registry = ObserverRegistry<TestObserver>()
        val observer = TestObserver()

        registry.register(observer, owner)

        // Observer gets notified
        assertFalse(observer.notified)
        registry.notifyObservers { somethingChanged() }
        assertTrue(observer.notified)

        // Pretend lifecycle gets destroyed
        mockedLifecycle.state = Lifecycle.State.DESTROYED
        mockedLifecycle.observer?.onStateChanged(null, null)

        observer.notified = false
        registry.notifyObservers { somethingChanged() }
        assertFalse(observer.notified)
    }

    @Test
    fun `non-destroy lifecycle changes do not affect observer`() {
        val mockedLifecycle = MockedLifecycle(Lifecycle.State.STARTED)
        val owner = MockedLifecycleOwner(mockedLifecycle)

        val registry = ObserverRegistry<TestObserver>()
        val observer = TestObserver()

        registry.register(observer, owner)

        // STARTED
        assertFalse(observer.notified)
        registry.notifyObservers { somethingChanged() }
        assertTrue(observer.notified)

        // RESUMED
        mockedLifecycle.state = Lifecycle.State.RESUMED
        mockedLifecycle.observer?.onStateChanged(null, null)
        observer.notified = false
        registry.notifyObservers { somethingChanged() }
        assertTrue(observer.notified)

        // CREATED
        mockedLifecycle.state = Lifecycle.State.CREATED
        mockedLifecycle.observer?.onStateChanged(null, null)
        observer.notified = false
        registry.notifyObservers { somethingChanged() }
        assertTrue(observer.notified)
    }

    @Test
    fun `unregisterObservers unregisters all observers`() {
        val registry = ObserverRegistry<TestObserver>()

        val observer1 = TestObserver()
        val observer2 = TestObserver()

        registry.register(observer1)
        registry.register(observer2)

        assertFalse(observer1.notified)
        assertFalse(observer2.notified)

        registry.notifyObservers { somethingChanged() }

        assertTrue(observer1.notified)
        assertTrue(observer2.notified)

        observer1.notified = false
        observer2.notified = false

        registry.unregisterObservers()

        registry.notifyObservers { somethingChanged() }

        assertFalse(observer1.notified)
        assertFalse(observer2.notified)
    }

    @Test
    fun `observer will not get added if view is detached`() {
        val view = mock(View::class.java)

        val registry = ObserverRegistry<TestObserver>()
        val observer = TestObserver()

        @Suppress("UsePropertyAccessSyntax")
        doReturn(false).`when`(view).isAttachedToWindow()

        registry.register(observer, view)

        assertFalse(observer.notified)

        registry.notifyObservers {
            somethingChanged()
        }

        assertFalse(observer.notified)
    }

    @Test
    fun `observer will get added once view is attached`() {
        val activity = Robolectric.buildActivity(Activity::class.java).create().get()
        val view = View(context)

        val registry = ObserverRegistry<TestObserver>()
        val observer = TestObserver()

        assertFalse(view.isAttachedToWindow)
        registry.register(observer, view)

        registry.notifyObservers {
            somethingChanged()
        }

        assertFalse(observer.notified)

        activity.windowManager.addView(view, WindowManager.LayoutParams(100, 100))
        assertTrue(view.isAttachedToWindow)

        registry.notifyObservers {
            somethingChanged()
        }

        assertTrue(observer.notified)
    }

    @Test
    fun `observer will get unregistered if view gets detached`() {
        val activity = Robolectric.buildActivity(Activity::class.java).create().get()
        val view = View(context)
        activity.windowManager.addView(view, WindowManager.LayoutParams(100, 100))

        val registry = ObserverRegistry<TestObserver>()
        val observer = TestObserver()

        assertTrue(view.isAttachedToWindow)

        registry.register(observer, view)

        assertFalse(observer.notified)

        registry.notifyObservers {
            somethingChanged()
        }

        assertTrue(observer.notified)

        observer.notified = false

        activity.windowManager.removeView(view)
        assertFalse(view.isAttachedToWindow)

        registry.notifyObservers {
            somethingChanged()
        }

        assertFalse(observer.notified)
    }

    @Test
    fun `wrapConsumers will return list of lambdas calling observers`() {
        val observer1 = TestConsumingObserver(shouldConsume = false)
        val observer2 = TestConsumingObserver(shouldConsume = true)
        val observer3 = TestConsumingObserver(shouldConsume = false)

        val registry = ObserverRegistry<TestConsumingObserver>()
        registry.register(observer1)
        registry.register(observer2)
        registry.register(observer3)

        val consumers: List<(Int) -> Boolean> = registry.wrapConsumers { value -> consumeSomething(value) }

        assertFalse(observer1.notified)
        assertFalse(observer2.notified)
        assertFalse(observer3.notified)

        val valueToConsume = 23
        consumers.forEach { it.invoke(valueToConsume) }

        assertTrue(observer1.notified)
        assertTrue(observer2.notified)
        assertTrue(observer3.notified)

        assertEquals(valueToConsume, observer1.notifiedWith!!)
        assertEquals(valueToConsume, observer2.notifiedWith!!)
        assertEquals(valueToConsume, observer3.notifiedWith!!)
    }

    @Test
    fun `observer is paused on lifecycle event (ON_PAUSE)`() {
        val mockedLifecycle = MockedLifecycle(Lifecycle.State.STARTED)
        val owner = MockedLifecycleOwner(mockedLifecycle)

        val registry = spy(ObserverRegistry<TestObserver>())

        val observer = TestObserver()
        registry.register(observer, owner, autoPause = false)

        val autoPausedObserver = TestObserver()
        registry.register(autoPausedObserver, owner, autoPause = true)

        // Both observers get notified
        assertFalse(observer.notified)
        assertFalse(autoPausedObserver.notified)
        registry.notifyObservers { somethingChanged() }
        assertTrue(observer.notified)
        assertTrue(autoPausedObserver.notified)

        // Pause observers
        mockedLifecycle.observer?.onStateChanged(null, Lifecycle.Event.ON_PAUSE)

        observer.notified = false
        autoPausedObserver.notified = false
        registry.notifyObservers { somethingChanged() }

        // (Regular) observer still gets notified
        verify(registry, never()).pauseObserver(observer)
        assertTrue(observer.notified)

        // (Auto-paused) observer is now paused and no longer gets notified
        verify(registry).pauseObserver(autoPausedObserver)
        assertFalse(autoPausedObserver.notified)
    }

    @Test
    fun `observer is resumed on lifecycle event (ON_RESUME)`() {
        val mockedLifecycle = MockedLifecycle(Lifecycle.State.STARTED)
        val owner = MockedLifecycleOwner(mockedLifecycle)

        val registry = spy(ObserverRegistry<TestIntObserver>())
        val observer = TestIntObserver()
        registry.register(observer, owner, autoPause = true)

        // Pause observer
        mockedLifecycle.observer?.onStateChanged(null, Lifecycle.Event.ON_PAUSE)
        registry.notifyObservers { somethingChanged(1) }
        registry.notifyObservers { somethingChanged(2) }
        registry.notifyObservers { somethingChanged(3) }
        assertEquals(emptyList<Int>(), observer.notified)

        // Resume observer
        mockedLifecycle.observer?.onStateChanged(null, Lifecycle.Event.ON_RESUME)

        // Resumed observer gets notified
        registry.notifyObservers { somethingChanged(4) }
        assertEquals(listOf(4), observer.notified)
        verify(registry).resumeObserver(observer)
    }

    private class TestObserver {
        var notified: Boolean = false

        fun somethingChanged() {
            notified = true
        }
    }

    private class TestIntObserver {
        var notified: List<Int> = listOf()

        fun somethingChanged(value: Int) {
            notified += value
        }
    }

    private class TestConsumingObserver(
        private val shouldConsume: Boolean
    ) {
        var notified: Boolean = false
        var notifiedWith: Int? = null

        fun consumeSomething(value: Int): Boolean {
            notifiedWith = value
            notified = true
            return shouldConsume
        }
    }

    private class MockedLifecycle(var state: State) : Lifecycle() {
        var observer: GenericLifecycleObserver? = null

        override fun addObserver(observer: LifecycleObserver) {
            this.observer = observer as GenericLifecycleObserver
        }

        override fun removeObserver(observer: LifecycleObserver) {
            this.observer = null
        }

        override fun getCurrentState(): State = state
    }

    private class MockedLifecycleOwner(private val lifecycle: MockedLifecycle) : LifecycleOwner {
        override fun getLifecycle(): Lifecycle = lifecycle
    }
}