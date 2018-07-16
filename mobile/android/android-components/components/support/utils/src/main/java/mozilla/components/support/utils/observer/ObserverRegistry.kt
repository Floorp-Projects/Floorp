/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, you can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.utils.observer

import android.arch.lifecycle.GenericLifecycleObserver
import android.arch.lifecycle.Lifecycle
import android.arch.lifecycle.LifecycleOwner
import android.view.View
import java.util.WeakHashMap

/**
 * A helper for classes that want to get observed. This class keeps track of registered observers
 * and can automatically unregister observers if a LifecycleOwner is provided.
 */
class ObserverRegistry<T> : Observable<T> {
    private val observers = mutableListOf<T>()
    private val lifecycleObservers = WeakHashMap<T, LifecycleBoundObserver<T>>()
    private val viewObservers = WeakHashMap<T, ViewBoundObserver<T>>()

    /**
     * Registers an observer to get notified about changes.
     */
    override fun register(observer: T) {
        synchronized(observers) {
            observers.add(observer)
        }
    }

    /**
     * Registers an observer to get notified about changes.
     *
     * The observer will automatically unsubscribe if the lifecycle of the provided LifecycleOwner
     * becomes DESTROYED.
     */
    override fun register(observer: T, owner: LifecycleOwner) {
        if (owner.lifecycle.currentState == Lifecycle.State.DESTROYED) {
            return
        }

        register(observer)

        val lifecycleObserver = LifecycleBoundObserver(
                owner,
                registry = this,
                observer = observer)

        lifecycleObservers[observer] = lifecycleObserver

        owner.lifecycle.addObserver(lifecycleObserver)
    }

    /**
     * Registers an observer to get notified about changes.
     *
     * The observer will automatically unsubscribe if the provided view gets detached.
     */
    override fun register(observer: T, view: View) {
        if (!view.isAttachedToWindow) {
            return
        }

        register(observer)

        val viewObserver = ViewBoundObserver(
            view,
            registry = this,
            observer = observer)

        viewObservers[observer] = viewObserver

        view.addOnAttachStateChangeListener(viewObserver)
    }

    /**
     * Unregisters an observer.
     */
    override fun unregister(observer: T) {
        synchronized(observers) {
            observers.remove(observer)
        }

        lifecycleObservers[observer]?.remove()
        viewObservers[observer]?.remove()
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
     * Returns a list of lambdas wrapping a consuming method of an observer.
     */
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

        override fun onViewAttachedToWindow(view: View) = Unit
    }
}
