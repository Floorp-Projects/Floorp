/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.push

import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleObserver
import androidx.lifecycle.LifecycleOwner
import mozilla.components.concept.push.Bus
import org.junit.Assert.assertEquals
import org.junit.Test

class MessageBusTest {

    @Test
    fun `register adds a new registry`() {
        val bus = MessageBus<TestType, String>()

        bus.register(TestType.FIRST, object : Bus.Observer<TestType, String> {
            override fun onEvent(type: TestType, message: String) {
                assertEquals("test", message)
            }
        })

        bus.notifyObservers(TestType.FIRST, "test")
    }

    @Test
    fun `register multiple observers to a registry`() {
        val bus = MessageBus<TestType, String>()
        var count = 0

        bus.register(TestType.FIRST, object : Bus.Observer<TestType, String> {
            override fun onEvent(type: TestType, message: String) {
                assertEquals("test", message)
                count++
            }
        })

        bus.register(TestType.FIRST, object : Bus.Observer<TestType, String> {
            override fun onEvent(type: TestType, message: String) {
                assertEquals("test", message)
                count++
            }
        })

        bus.notifyObservers(TestType.FIRST, "test")
        assertEquals(2, count)
    }

    @Test
    fun `register with lifecycle owner adds a new register`() {
        val bus = MessageBus<TestType, String>()

        bus.register(TestType.FIRST, object : Bus.Observer<TestType, String> {
            override fun onEvent(type: TestType, message: String) {
                assertEquals("test", message)
            }
        }, MockedLifecycleOwner(MockedLifecycle(Lifecycle.State.STARTED)), false)

        bus.notifyObservers(TestType.FIRST, "test")
    }

    @Test
    fun `register with lifecycle owner and multiple observers to a register`() {
        val bus = MessageBus<TestType, String>()
        var count = 0

        bus.register(TestType.FIRST, object : Bus.Observer<TestType, String> {
            override fun onEvent(type: TestType, message: String) {
                assertEquals("test", message)
                count++
            }
        }, MockedLifecycleOwner(MockedLifecycle(Lifecycle.State.STARTED)), false)

        bus.register(TestType.FIRST, object : Bus.Observer<TestType, String> {
            override fun onEvent(type: TestType, message: String) {
                assertEquals("test", message)
                count++
            }
        }, MockedLifecycleOwner(MockedLifecycle(Lifecycle.State.STARTED)), false)

        bus.notifyObservers(TestType.FIRST, "test")
        assertEquals(2, count)
    }

    @Test
    fun `observer on one registry does not notify another`() {
        val bus = MessageBus<TestType, String>()
        var count = 0

        bus.register(TestType.FIRST, object : Bus.Observer<TestType, String> {
            override fun onEvent(type: TestType, message: String) {
                assertEquals("test", message)
                count++
            }
        }, MockedLifecycleOwner(MockedLifecycle(Lifecycle.State.STARTED)), false)

        bus.register(TestType.SECOND, object : Bus.Observer<TestType, String> {
            override fun onEvent(type: TestType, message: String) {
                count++
                throw IllegalStateException("this should never happen")
            }
        }, MockedLifecycleOwner(MockedLifecycle(Lifecycle.State.STARTED)), false)

        bus.notifyObservers(TestType.FIRST, "test")

        assertEquals(1, count)
    }

    @Test
    fun `observer on separate registries notify both individually`() {
        val bus = MessageBus<TestType, String>()
        var count = 0

        bus.register(TestType.FIRST, object : Bus.Observer<TestType, String> {
            override fun onEvent(type: TestType, message: String) {
                assertEquals("test", message)
                count++
            }
        }, MockedLifecycleOwner(MockedLifecycle(Lifecycle.State.STARTED)), false)

        bus.register(TestType.SECOND, object : Bus.Observer<TestType, String> {
            override fun onEvent(type: TestType, message: String) {
                assertEquals("test2", message)
                count++
            }
        }, MockedLifecycleOwner(MockedLifecycle(Lifecycle.State.STARTED)), false)

        bus.notifyObservers(TestType.FIRST, "test")
        bus.notifyObservers(TestType.SECOND, "test2")

        assertEquals(2, count)
    }

    @Test
    fun `unregister removes observer from registry`() {
        val bus = MessageBus<TestType, String>()
        var count = 0
        val observer = object : Bus.Observer<TestType, String> {
            override fun onEvent(type: TestType, message: String) {
                assertEquals("test", message)
                count++
            }
        }

        bus.register(TestType.FIRST, observer, MockedLifecycleOwner(MockedLifecycle(Lifecycle.State.STARTED)), false)

        bus.notifyObservers(TestType.FIRST, "test")

        bus.unregister(TestType.FIRST, observer)

        bus.notifyObservers(TestType.FIRST, "test")

        assertEquals(1, count)
    }

    @Test
    fun `unregister removes only the specified observer from a registry`() {
        val bus = MessageBus<TestType, String>()
        var count = 0
        val observer = object : Bus.Observer<TestType, String> {
            override fun onEvent(type: TestType, message: String) {
                assertEquals("test", message)
                count++
            }
        }
        val observer2 = object : Bus.Observer<TestType, String> {
            override fun onEvent(type: TestType, message: String) {
                assertEquals("test", message)
                count++
            }
        }

        bus.register(TestType.FIRST, observer)
        bus.register(TestType.FIRST, observer2)

        bus.notifyObservers(TestType.FIRST, "test")

        bus.unregister(TestType.FIRST, observer)

        bus.notifyObservers(TestType.FIRST, "test")

        assertEquals(3, count)
    }

    @Test
    fun `unregister removes only the specified observer from a specified registry`() {
        val bus = MessageBus<TestType, String>()
        var count = 0
        val observer = object : Bus.Observer<TestType, String> {
            override fun onEvent(type: TestType, message: String) {
                assertEquals("test", message)
                count++
            }
        }
        val observer2 = object : Bus.Observer<TestType, String> {
            override fun onEvent(type: TestType, message: String) {
                assertEquals("test", message)
                count++
            }
        }

        bus.register(TestType.FIRST, observer)
        bus.register(TestType.SECOND, observer2)

        bus.notifyObservers(TestType.FIRST, "test")

        bus.unregister(TestType.FIRST, observer)

        bus.notifyObservers(TestType.SECOND, "test")

        assertEquals(2, count)
    }

    private class MockedLifecycle(var state: State) : Lifecycle() {
        var observer: LifecycleObserver? = null

        override fun addObserver(observer: LifecycleObserver) {
            this.observer = observer
        }

        override fun removeObserver(observer: LifecycleObserver) {
            this.observer = null
        }

        override fun getCurrentState(): State = state
    }

    private class MockedLifecycleOwner(private val lifecycle: MockedLifecycle) : LifecycleOwner {
        override fun getLifecycle(): Lifecycle = lifecycle
    }

    enum class TestType {
        FIRST,
        SECOND
    }
}
