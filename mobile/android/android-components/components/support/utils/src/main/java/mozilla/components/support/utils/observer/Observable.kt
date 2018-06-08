package mozilla.components.support.utils.observer

import android.arch.lifecycle.LifecycleOwner

/**
 * Interface for observables. This interface is implemented by ObserverRegistry so that classes that
 * want to be observable can implement the interface by delegation:
 *
 * <code>
 *     val registry = ObserverRegistry<MyObserverInterface>()
 *
 *     class MyObservableClass : Observable<MyObserverInterface> by registry {
 *       ...
 *     }
 * </code>
 */
interface Observable<T> {
    /**
     * Registers an observer to get notified about changes.
     *
     * Optionally a LifecycleOwner can be provided. Once the lifecycle state becomes DESTROYED the
     * observer is automatically unregistered.
     */
    fun register(observer: T, owner: LifecycleOwner? = null)

    /**
     * Unregisters an observer.
     */
    fun unregister(observer: T)

    /**
     * Unregisters all observers.
     */
    fun unregisterObservers()

    /**
     * Notify all registered observers about a change.
     */
    fun notifyObservers(block: T.() -> Unit)
}
