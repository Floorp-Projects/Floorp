/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.base.observer

import android.view.View
import androidx.lifecycle.LifecycleOwner

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
     * @param observer the observer to register.
     */
    fun register(observer: T)

    /**
     * Registers an observer to get notified about changes.
     *
     * The observer will automatically unsubscribe if the lifecycle of the provided LifecycleOwner
     * becomes DESTROYED.
     *
     * @param observer the observer to register.
     * @param owner the lifecycle owner the provided observer is bound to.
     * @param autoPause whether or not the observer should automatically be
     * paused/resumed with the bound lifecycle.
     */
    fun register(observer: T, owner: LifecycleOwner, autoPause: Boolean = false)

    /**
     * Registers an observer to get notified about changes.
     *
     * The observer will only be notified if the view is attached and will be unregistered/
     * registered if the attached state changes.
     *
     * @param observer the observer to register.
     * @param view the view the provided observer is bound to.
     */
    fun register(observer: T, view: View)

    /**
     * Unregisters an observer.
     *
     * @param observer the observer to unregister.
     */
    fun unregister(observer: T)

    /**
     * Unregisters all observers.
     */
    fun unregisterObservers()

    /**
     * Notifies all registered observers about a change.
     *
     * @param block the notification (method on the observer to be invoked).
     */
    fun notifyObservers(block: T.() -> Unit)

    /**
     * Notifies all registered observers about a change. If there is no observer
     * the notification is queued and sent to the first observer that is
     * registered.
     *
     * @param block the notification (method on the observer to be invoked).
     */
    fun notifyAtLeastOneObserver(block: T.() -> Unit)

    /**
     * Pauses the provided observer. No notifications will be sent to this
     * observer until [resumeObserver] is called.
     *
     * @param observer the observer to pause.
     */
    fun pauseObserver(observer: T)

    /**
     * Resumes the provided observer. Notifications sent since it
     * was last paused (see [pauseObserver]]) are lost and will not be
     * re-delivered.
     *
     * @param observer the observer to resume.
     */
    fun resumeObserver(observer: T)

    /**
     * Returns a list of lambdas wrapping a consuming method of an observer.
     */
    fun <R> wrapConsumers(block: T.(R) -> Boolean): List<(R) -> Boolean>

    /**
     * If the observable has registered observers.
     */
    fun isObserved(): Boolean
}

/**
 * A deprecated version of [Observable] to migrate and deprecate existing
 * components individually. All components implement [Observable] by delegate
 * so this makes it easy to deprecate without having to override all methods
 * in each component separately.
 */
@Deprecated(OBSERVER_DEPRECATION_MESSAGE)
interface DeprecatedObservable<T> : Observable<T> {

    @Deprecated(OBSERVER_DEPRECATION_MESSAGE)
    override fun register(observer: T)

    @Deprecated(OBSERVER_DEPRECATION_MESSAGE)
    override fun register(observer: T, owner: LifecycleOwner, autoPause: Boolean)

    @Deprecated(OBSERVER_DEPRECATION_MESSAGE)
    override fun register(observer: T, view: View)

    @Deprecated(OBSERVER_DEPRECATION_MESSAGE)
    override fun unregister(observer: T)

    @Deprecated(OBSERVER_DEPRECATION_MESSAGE)
    override fun unregisterObservers()

    @Deprecated(OBSERVER_DEPRECATION_MESSAGE)
    override fun notifyObservers(block: T.() -> Unit)

    @Deprecated(OBSERVER_DEPRECATION_MESSAGE)
    override fun notifyAtLeastOneObserver(block: T.() -> Unit)

    @Deprecated(OBSERVER_DEPRECATION_MESSAGE)
    override fun pauseObserver(observer: T)

    @Deprecated(OBSERVER_DEPRECATION_MESSAGE)
    override fun resumeObserver(observer: T)

    @Deprecated(OBSERVER_DEPRECATION_MESSAGE)
    override fun <R> wrapConsumers(block: T.(R) -> Boolean): List<(R) -> Boolean>

    @Deprecated(OBSERVER_DEPRECATION_MESSAGE)
    override fun isObserved(): Boolean
}

const val OBSERVER_DEPRECATION_MESSAGE =
    "Use browser store (browser-state component) for observing state changes instead"
