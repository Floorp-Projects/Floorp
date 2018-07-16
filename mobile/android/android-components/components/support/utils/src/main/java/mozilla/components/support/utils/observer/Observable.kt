package mozilla.components.support.utils.observer

import android.arch.lifecycle.LifecycleOwner
import android.view.View

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
     */
    fun register(observer: T)

    /**
     * Registers an observer to get notified about changes.
     *
     * The observer will automatically unsubscribe if the lifecycle of the provided LifecycleOwner
     * becomes DESTROYED.
     */
    fun register(observer: T, owner: LifecycleOwner)

    /**
     * Registers an observer to get notified about changes.
     *
     * The observer will automatically unsubscribe if the provided view gets detached.
     */
    fun register(observer: T, view: View)

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

    /**
     * Returns a list of lambdas wrapping a consuming method of an observer.
     */
    fun <R> wrapConsumers(block: T.(R) -> Boolean): List<(R) -> Boolean>
}
