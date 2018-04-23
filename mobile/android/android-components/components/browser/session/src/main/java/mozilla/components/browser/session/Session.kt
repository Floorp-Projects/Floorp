/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.session

import kotlin.properties.Delegates

/**
 * Value type that represents the state of a browser session. Changes can be observed.
 */
class Session(
    initialUrl: String
) {
    /**
     * Interface to be implemented by classes that want to observe a session.
     */
    interface Observer {
        fun onUrlChanged()
        fun onProgress()
        fun onLoadingStateChanged()
    }

    private val observers = mutableListOf<Observer>()

    /**
     * The currently loading or loaded URL.
     */
    var url: String by Delegates.observable(initialUrl) {
        _, _, _ -> notifyObservers { onUrlChanged() }
    }

    /**
     * The progress loading the current URL.
     */
    var progress: Int by Delegates.observable(0) {
        _, _, _ -> notifyObservers { onProgress() }
    }

    /**
     * Loading state, true if this session's url is currently loading, otherwise false.
     */
    var loading: Boolean by Delegates.observable(false) {
        _, _, _ -> notifyObservers { onLoadingStateChanged() }
    }

    /**
     * Register an observer that gets notified when the session changes.
     */
    fun register(observer: Observer) = synchronized(observers) {
        observers.add(observer)
    }

    /**
     * Unregister an observer.
     */
    fun unregister(observer: Observer) = synchronized(observers) {
        observers.remove(observer)
    }

    /**
     * Helper method to notify observers.
     */
    internal fun notifyObservers(block: Observer.() -> Unit) = synchronized(observers) {
        observers.forEach { it.block() }
    }
}
