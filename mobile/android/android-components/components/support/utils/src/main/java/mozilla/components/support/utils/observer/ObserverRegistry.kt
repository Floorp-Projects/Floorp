/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, you can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.utils.observer

import android.arch.lifecycle.GenericLifecycleObserver
import android.arch.lifecycle.Lifecycle
import android.arch.lifecycle.LifecycleOwner
import java.util.WeakHashMap

/**
 * A helper for classes that want to get observed. This class keeps track of registered observers
 * and can automatically unregister observers if a LifecycleOwner is provided.
 */
class ObserverRegistry<T> : Observable<T> {
    private val observers = mutableListOf<T>()
    private val lifecycleObservers = WeakHashMap<T, LifecycleBoundObserver<T>>()

    /**
     * Registers an observer to get notified about changes.
     *
     * Optionally a LifecycleOwner can be provided. Once the lifecycle state becomes DESTROYED the
     * observer is automatically unregistered.
     */
    override fun register(observer: T, owner: LifecycleOwner?) {
        if (owner?.lifecycle?.currentState == Lifecycle.State.DESTROYED) {
            return
        }

        synchronized(observers) {
            observers.add(observer)
        }

        owner?.let {
            val lifecycleObserver = LifecycleBoundObserver(
                    owner,
                    registry = this,
                    observer = observer)

            lifecycleObservers[observer] = lifecycleObserver

            it.lifecycle.addObserver(lifecycleObserver)
        }
    }

    /**
     * Unregisters an observer.
     */
    override fun unregister(observer: T) {
        synchronized(observers) {
            observers.remove(observer)
        }

        lifecycleObservers[observer]?.remove()
    }

    /**
     * Unregisters all observers.
     */
    override fun unregisterObservers() {
        synchronized(observers) {
            observers.forEach {
                lifecycleObservers[it]?.remove()
            }
            observers.clear()
        }
    }

    /**
     * Notify all registered observers about a change.
     */
    override fun notifyObservers(block: T.() -> Unit) {
        synchronized(observers) {
            observers.forEach {
                it.block()
            }
        }
    }

    /**
     * GenericLifecycleObserver implementation to bind an observer to a Lifecycle.
     */
    private class LifecycleBoundObserver<T>(
        private val owner: LifecycleOwner,
        private val registry: ObserverRegistry<T>,
        private val observer: T
    ) : GenericLifecycleObserver {
        override fun onStateChanged(source: LifecycleOwner?, event: Lifecycle.Event?) {
            if (owner.lifecycle.currentState == Lifecycle.State.DESTROYED) {
                registry.unregister(observer)
            }
        }

        fun remove() {
            owner.lifecycle.removeObserver(this)
        }
    }
}
