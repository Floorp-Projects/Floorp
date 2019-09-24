/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.push

import androidx.lifecycle.LifecycleOwner
import mozilla.components.concept.push.Bus
import mozilla.components.support.base.observer.ObserverRegistry

typealias Registry<T, M> = ObserverRegistry<Bus.Observer<T, M>>

/**
 * An implementation of [Bus] where the event type is restricted to an enum.
 */
class MessageBus<T : Enum<T>, M> : Bus<T, M> {

    private val bus: HashMap<T, Registry<T, M>> = hashMapOf()

    override fun register(type: T, observer: Bus.Observer<T, M>) {
        synchronized(bus) {
            val registry = bus[type]

            if (registry != null) {
                registry.register(observer)
                return
            }

            val newRegistry = Registry<T, M>()
            newRegistry.register(observer)
            bus[type] = newRegistry
        }
    }

    override fun register(type: T, observer: Bus.Observer<T, M>, owner: LifecycleOwner, autoPause: Boolean) {
        synchronized(bus) {
            val registry = bus[type]

            if (registry != null) {
                registry.register(observer, owner, autoPause)
                return
            }

            val newRegistry = Registry<T, M>()
            newRegistry.register(observer, owner, autoPause)
            bus[type] = newRegistry
        }
    }

    override fun unregister(type: T, observer: Bus.Observer<T, M>) {
        runOnRegistryOfType(type) { registry ->
            registry.unregister(observer)

            if (!registry.isObserved()) {
                bus.remove(type)
            }
        }
    }

    override fun notifyObservers(type: T, message: M) {
        runOnRegistryOfType(type) { bus ->
            bus.notifyObservers { onEvent(type, message) }
        }
    }

    private inline fun runOnRegistryOfType(type: T, block: (Registry<T, M>) -> Unit) {
        synchronized(bus) {
            val registry = bus[type]
            if (registry != null) {
                block(registry)
            }
        }
    }
}
