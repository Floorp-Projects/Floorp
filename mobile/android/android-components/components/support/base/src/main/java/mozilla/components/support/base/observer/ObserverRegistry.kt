/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, you can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.base.observer

import android.view.View
import androidx.lifecycle.GenericLifecycleObserver
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleOwner
import java.util.Collections
import java.util.WeakHashMap

/**
 * A helper for classes that want to get observed. This class keeps track of registered observers
 * and can automatically unregister observers if a LifecycleOwner is provided.
 */
class ObserverRegistry<T> : Observable<T> {
    private val observers = mutableListOf<T>()
    private val lifecycleObservers = WeakHashMap<T, LifecycleBoundObserver<T>>()
    private val viewObservers = WeakHashMap<T, ViewBoundObserver<T>>()
    private val pausedObservers = Collections.newSetFromMap(WeakHashMap<T, Boolean>())

    override fun register(observer: T) {
        synchronized(observers) {
            observers.add(observer)
        }
    }

    override fun register(observer: T, owner: LifecycleOwner, autoPause: Boolean) {
        if (owner.lifecycle.currentState == Lifecycle.State.DESTROYED) {
            return
        }

        register(observer)

        val lifecycleObserver = LifecycleBoundObserver(
                owner,
                registry = this,
                observer = observer,
                autoPause = autoPause)

        lifecycleObservers[observer] = lifecycleObserver

        owner.lifecycle.addObserver(lifecycleObserver)
    }

    override fun register(observer: T, view: View) {
        val viewObserver = ViewBoundObserver(
                view,
                registry = this,
                observer = observer)

        viewObservers[observer] = viewObserver

        view.addOnAttachStateChangeListener(viewObserver)

        if (view.isAttachedToWindow) {
            register(observer)
        }
    }

    override fun unregister(observer: T) {
        synchronized(observers) {
            observers.remove(observer)
            pausedObservers.remove(observer)
        }

        lifecycleObservers[observer]?.remove()
        viewObservers[observer]?.remove()
    }

    override fun unregisterObservers() {
        synchronized(observers) {
            observers.forEach {
                lifecycleObservers[it]?.remove()
            }
            observers.clear()
            pausedObservers.clear()
        }
    }

    override fun pauseObserver(observer: T) {
        synchronized(observers) {
            pausedObservers.add(observer)
        }
    }

    override fun resumeObserver(observer: T) {
        synchronized(observers) {
            pausedObservers.remove(observer)
        }
    }

    override fun notifyObservers(block: T.() -> Unit) {
        synchronized(observers) {
            observers.forEach {
                if (!pausedObservers.contains(it)) {
                    it.block()
                }
            }
        }
    }

    override fun <V> wrapConsumers(block: T.(V) -> Boolean): List<(V) -> Boolean> {
        val consumers: MutableList<(V) -> Boolean> = mutableListOf()

        synchronized(observers) {
            observers.forEach { observer ->
                consumers.add { value -> observer.block(value) }
            }
        }

        return consumers
    }

    /**
     * GenericLifecycleObserver implementation to bind an observer to a Lifecycle.
     */
    private class LifecycleBoundObserver<T>(
        private val owner: LifecycleOwner,
        private val registry: ObserverRegistry<T>,
        private val observer: T,
        private val autoPause: Boolean
    ) : GenericLifecycleObserver {
        override fun onStateChanged(source: LifecycleOwner?, event: Lifecycle.Event?) {
            if (autoPause) {
                if (event == Lifecycle.Event.ON_PAUSE) {
                    registry.pauseObserver(observer)
                } else if (event == Lifecycle.Event.ON_RESUME) {
                    registry.resumeObserver(observer)
                }
            }

            if (owner.lifecycle.currentState == Lifecycle.State.DESTROYED) {
                registry.unregister(observer)
            }
        }

        fun remove() {
            owner.lifecycle.removeObserver(this)
        }
    }

    /**
     * View.OnAttachStateChangeListener implementation to automatically unregister an observer if
     * the bound view gets detached.
     */
    private class ViewBoundObserver<T>(
        private val view: View,
        private val registry: ObserverRegistry<T>,
        private val observer: T
    ) : View.OnAttachStateChangeListener {
        override fun onViewDetachedFromWindow(view: View) {
            registry.unregister(observer)
        }

        fun remove() {
            view.removeOnAttachStateChangeListener(this)
        }

        override fun onViewAttachedToWindow(view: View) {
            registry.register(observer)
        }
    }
}
