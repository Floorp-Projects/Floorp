/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.base.observer

import android.view.View
import androidx.annotation.MainThread
import androidx.annotation.VisibleForTesting
import androidx.lifecycle.DefaultLifecycleObserver
import androidx.lifecycle.Lifecycle.State.DESTROYED
import androidx.lifecycle.Lifecycle.State.RESUMED
import androidx.lifecycle.LifecycleOwner
import java.util.Collections
import java.util.LinkedList
import java.util.WeakHashMap

/**
 * A helper for classes that want to get observed. This class keeps track of registered observers
 * and can automatically unregister observers if a LifecycleOwner is provided.
 *
 * ObserverRegistry is thread-safe.
 */
open class ObserverRegistry<T> : Observable<T> {
    private val observers = mutableSetOf<T>()
    private val lifecycleObservers = WeakHashMap<T, DefaultLifecycleObserver>()
    private val viewObservers = WeakHashMap<T, ViewBoundObserver<T>>()
    private val pausedObservers = Collections.newSetFromMap(WeakHashMap<T, Boolean>())
    private val queuedNotifications = LinkedList<T.() -> Unit>()

    /**
     * Registers an observer to get notified about changes. Does nothing if [observer] is already registered.
     * This method is thread-safe.
     *
     * @param observer the observer to register.
     */
    @Synchronized
    open override fun register(observer: T) {
        observers.add(observer)

        while (!queuedNotifications.isEmpty()) {
            queuedNotifications.poll()?.let { notify -> observer.notify() }
        }
    }

    @Synchronized
    @MainThread
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

        // In newer AndroidX versions of the lifecycle lib, the default requirement is for
        // lifecycleRegistry to be only touched on the main thread. We don't know if `onwer`'s
        // lifecycle registry was created with or without this requirement, but assume so since
        // that's the default and also the reasonable thing to do.
        owner.lifecycle.addObserver(lifecycleObserver)
    }

    @Synchronized
    override fun register(observer: T, view: View) {
        val viewObserver = ViewBoundObserver(
            view,
            registry = this,
            observer = observer,
        )

        viewObservers[observer] = viewObserver

        view.addOnAttachStateChangeListener(viewObserver)

        if (view.isAttachedToWindow) {
            register(observer)
        }
    }

    /**
     * Unregisters an observer. Does nothing if [observer] is not registered.
     *
     * @param observer the observer to unregister.
     */
    @Synchronized
    override fun unregister(observer: T) {
        // Remove observer
        observers.remove(observer)
        pausedObservers.remove(observer)

        // Unregister view observers
        viewObservers[observer]?.remove()

        // Remove lifecycle/view observers from map
        lifecycleObservers.remove(observer)
        viewObservers.remove(observer)
    }

    @Synchronized
    override fun unregisterObservers() {
        // Remove all registered observers
        observers.toList().forEach { observer ->
            unregister(observer)
        }

        // There can still be view observers for views that are not attached yet and therefore the observers were not
        // registered yet. Let's remove them too.
        viewObservers.keys.toList().forEach { observer ->
            unregister(observer)
        }

        // If any of our sets and maps is not empty now then this would be a serious bug.
        checkInternalCollectionsAreEmpty()
    }

    @Synchronized
    override fun pauseObserver(observer: T) {
        pausedObservers.add(observer)
    }

    @Synchronized
    override fun resumeObserver(observer: T) {
        pausedObservers.remove(observer)
    }

    @Synchronized
    override fun notifyObservers(block: T.() -> Unit) {
        observers.forEach {
            if (!pausedObservers.contains(it)) {
                it.block()
            }
        }
    }

    @Synchronized
    override fun notifyAtLeastOneObserver(block: T.() -> Unit) {
        if (observers.isEmpty()) {
            queuedNotifications.add(block)
        } else {
            notifyObservers(block)
        }
    }

    @Synchronized
    override fun <V> wrapConsumers(block: T.(V) -> Boolean): List<(V) -> Boolean> {
        val consumers: MutableList<(V) -> Boolean> = mutableListOf()

        observers.forEach { observer ->
            consumers.add { value -> observer.block(value) }
        }

        return consumers
    }

    @Synchronized
    override fun isObserved(): Boolean {
        // The registry is getting observed if there are registered observer or if there are registered view observers
        // that will register an observer as soon as their views are attached.
        return observers.isNotEmpty() || viewObservers.isNotEmpty()
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal fun checkInternalCollectionsAreEmpty(): Boolean {
        check(observers.isEmpty())
        check(pausedObservers.isEmpty())
        check(lifecycleObservers.isEmpty())
        check(viewObservers.isEmpty())
        return true
    }

    /**
     * LifecycleObserver implementation to bind an observer to a Lifecycle.
     */
    private open class LifecycleBoundObserver<T>(
        private val owner: LifecycleOwner,
        protected val registry: ObserverRegistry<T>,
        protected val observer: T,
    ) : DefaultLifecycleObserver {

        @MainThread
        fun remove() {
            // In newer AndroidX versions of the lifecycle lib, the default requirement is for
            // lifecycleRegistry to be only touched on the main thread. We don't know if `onwer`'s
            // lifecycle registry was created with or without this requirement, but assume so since
            // that's the default and also the reasonable thing to do.
            owner.lifecycle.removeObserver(this)
        }

        override fun onDestroy(owner: LifecycleOwner) {
            registry.unregister(observer)
        }
    }

    /**
     * LifecycleObserver implementation to bind an observer to a Lifecycle and pause observing
     * automatically for the pause and resume events.
     */
    private class AutoPauseLifecycleBoundObserver<T>(
        owner: LifecycleOwner,
        private val registry: ObserverRegistry<T>,
        private val observer: T,
    ) : DefaultLifecycleObserver {
        init {
            if (!owner.lifecycle.currentState.isAtLeast(RESUMED)) {
                registry.pauseObserver(observer)
            }
        }

        override fun onResume(owner: LifecycleOwner) {
            registry.resumeObserver(observer)
        }

        override fun onPause(owner: LifecycleOwner) {
            registry.pauseObserver(observer)
        }
    }

    /**
     * View.OnAttachStateChangeListener implementation to automatically unregister an observer if
     * the bound view gets detached.
     */
    private class ViewBoundObserver<T>(
        private val view: View,
        private val registry: ObserverRegistry<T>,
        private val observer: T,
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

/**
 * A deprecated version of [ObserverRegistry] to migrate and deprecate existing
 * components individually. All components implement [ObserverRegistry] by
 * delegate so this makes it easy to deprecate without having to override
 * all methods in each component separately.
 */
@Deprecated(OBSERVER_DEPRECATION_MESSAGE)
@Suppress("Deprecation")
class DeprecatedObserverRegistry<T> : ObserverRegistry<T>(), DeprecatedObservable<T>
