/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, you can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.utils.observer

import android.app.Activity
import android.arch.lifecycle.GenericLifecycleObserver
import android.arch.lifecycle.Lifecycle
import android.arch.lifecycle.LifecycleObserver
import android.arch.lifecycle.LifecycleOwner
import android.view.View
import android.view.WindowManager
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.mock
import org.robolectric.Robolectric
import org.robolectric.RobolectricTestRunner
import org.robolectric.RuntimeEnvironment

@RunWith(RobolectricTestRunner::class)
class ObserverRegistryTest {
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
    fun `observer will get unregistered if view gets detached`() {
        val activity = Robolectric.buildActivity(Activity::class.java).create().get()
        val view = View(RuntimeEnvironment.application)
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

    private class TestObserver {
        var notified: Boolean = false

        fun somethingChanged() {
            notified = true
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