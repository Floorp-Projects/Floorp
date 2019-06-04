/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, you can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.base.observer

import android.view.View
import androidx.lifecycle.Lifecycle.Event.ON_DESTROY
import androidx.lifecycle.Lifecycle.Event.ON_PAUSE
import androidx.lifecycle.Lifecycle.Event.ON_RESUME
import androidx.lifecycle.Lifecycle.State.DESTROYED
import androidx.lifecycle.Lifecycle.State.RESUMED
import androidx.lifecycle.LifecycleObserver
import androidx.lifecycle.LifecycleOwner
import androidx.lifecycle.OnLifecycleEvent
import java.util.Collections
import java.util.WeakHashMap

/**
 * A helper for classes that want to get observed. This class keeps track of registered observers
 * and can automatically unregister observers if a LifecycleOwner is provided.
 */
class ObserverRegistry<T> : Observable<T> {
    private val observers = mutableSetOf<T>()
    private val lifecycleObservers = WeakHashMap<T, LifecycleBoundObserver<T>>()
    private val viewObservers = WeakHashMap<T, ViewBoundObserver<T>>()
    private val pausedObservers = Collections.newSetFromMap(WeakHashMap<T, Boolean>())

    /**
     * Registers an observer to get notified about changes. Does nothing if [observer] is already registered.
     * This method is thread-safe.
     *
     * @param observer the observer to register.
     */
    override fun register(observer: T) {
        synchronized(observers) {
            observers.add(observer)
        }
    }

    override fun register(observer: T, owner: LifecycleOwner, autoPause: Boolean) {
        // Don't register if the owner is already destroyed
        if (owner.lifecycle.currentState == DESTROYED) {
            return
        }

        register(observer)

        val lifecycleObserver = if (autoPause) {
            AutoPauseLifecycleBoundObserver(owner, registry = this, observer = observer)
        } else {
            LifecycleBoundObserver(owner, registry = this, observer = observer)
        }

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

    /**
     * Unregisters an observer. Does nothing if [observer] is not registered.
     * This method is thread-safe.
     *
     * @param observer the observer to unregister.
     */
    override fun unregister(observer: T) {
        synchronized(observers) {
            observers.remove(observer)
            pausedObservers.remove(observer)
        }

        // Unregister observers
        lifecycleObservers[observer]?.remove()
        viewObservers[observer]?.remove()
        // Remove observers from map
        lifecycleObservers.remove(observer)
        viewObservers.remove(observer)
    }

    override fun unregisterObservers() {
        synchronized(observers) {
            observers.forEach {
                lifecycleObservers[it]?.remove()
            }
            observers.clear()
            pausedObservers.clear()
            lifecycleObservers.clear()
            viewObservers.clear()
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

    override fun isObserved(): Boolean {
        synchronized(observers) {
            return !observers.isEmpty()
        }
    }

    /**
     * LifecycleObserver implementation to bind an observer to a Lifecycle.
     */
    private open class LifecycleBoundObserver<T>(
        private val owner: LifecycleOwner,
        protected val registry: ObserverRegistry<T>,
        protected val observer: T
    ) : LifecycleObserver {
        @OnLifecycleEvent(ON_DESTROY)
        fun onDestroy() = registry.unregister(observer)

        fun remove() = owner.lifecycle.removeObserver(this)
    }

    /**
     * LifecycleObserver implementation to bind an observer to a Lifecycle and pause observing
     * automatically for the pause and resume events.
     */
    private class AutoPauseLifecycleBoundObserver<T>(
        owner: LifecycleOwner,
        registry: ObserverRegistry<T>,
        observer: T
    ) : LifecycleBoundObserver<T>(owner, registry, observer) {
        init {
            if (!owner.lifecycle.currentState.isAtLeast(RESUMED)) {
                registry.pauseObserver(observer)
            }
        }

        @OnLifecycleEvent(ON_PAUSE)
        fun onPause() = registry.pauseObserver(observer)

        @OnLifecycleEvent(ON_RESUME)
        fun onResume() = registry.resumeObserver(observer)
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
