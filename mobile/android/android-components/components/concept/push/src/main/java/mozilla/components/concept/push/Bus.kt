/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.push

import androidx.lifecycle.LifecycleOwner

/**
 * Interface for a basic bus that is implemented by MessageBus so that classes can observe particular event types.
 *
 * <code>
 *     enum class Type { A, B }
 *     data class Message(val datum: String)
 *     val bus = MessageBus<Type, Message>()
 *     class MyObservableClass : Bus.Observer<Type, Message> {
 *          override fun onEvent(type: Type, msg: Message) {
 *              // Handle message here.
 *          }
 *     }
 *
 *     bus.register(Type.A, MyObservableClass())
 *
 *     // In some other class, we can notify the MyObservableClass.
 *     bus.notifyObservers(Type.A, Message("Hello!"))
 * </code>
 *
 * @param T A type that's capable of describing message categories (e.g. an enum).
 * @param M A type for holding message contents (e.g. data or sealed class).
 */
interface Bus<T, M> {

    /**
     * Registers an observer to get notified about events.
     *
     * @param observer The observer to be notified for the type.
     */
    fun register(type: T, observer: Bus.Observer<T, M>)

    /**
     * Registers an observer to get notified about events.
     *
     * @param observer The observer to be notified for the type.
     * @param owner the lifecycle owner the provided observer is bound to.
     * @param autoPause whether or not the observer should automatically be
     * paused/resumed with the bound lifecycle.
     */
    fun register(type: T, observer: Bus.Observer<T, M>, owner: LifecycleOwner, autoPause: Boolean)

    /**
     * Unregisters an observer to stop getting notified about events.
     *
     * @param observer The observer that no longer wants to be notified.
     */
    fun unregister(type: T, observer: Bus.Observer<T, M>)

    /**
     * Notifies all registered observers of a particular message.
     */
    fun notifyObservers(type: T, message: M)

    /**
     * An observer interface that all listeners have to implement in order to register and receive events.
     */
    interface Observer<T, M> {
        fun onEvent(type: T, message: M)
    }
}
