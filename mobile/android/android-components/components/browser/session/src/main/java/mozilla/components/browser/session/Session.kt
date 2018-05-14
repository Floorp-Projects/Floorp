/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.session

import java.util.UUID
import kotlin.properties.Delegates

/**
 * Value type that represents the state of a browser session. Changes can be observed.
 */
class Session(
    initialUrl: String,
    val id: String = UUID.randomUUID().toString()
) {
    /**
     * Interface to be implemented by classes that want to observe a session.
     */
    interface Observer {
        fun onUrlChanged()
        fun onProgress()
        fun onLoadingStateChanged()
        fun onNavigationStateChanged()
    }

    private val observers = mutableListOf<Observer>()

    /**
     * The currently loading or loaded URL.
     */
    var url: String by Delegates.observable(initialUrl) {
        _, old, new -> notifyObservers (old, new, { onUrlChanged() })
    }

    /**
     * The progress loading the current URL.
     */
    var progress: Int by Delegates.observable(0) {
        _, old, new -> notifyObservers (old, new, { onProgress() })
    }

    /**
     * Loading state, true if this session's url is currently loading, otherwise false.
     */
    var loading: Boolean by Delegates.observable(false) {
        _, old, new -> notifyObservers (old, new, { onLoadingStateChanged() })
    }

    /**
     * Navigation state, true if there's an history item to go back to, otherwise false.
     */
    var canGoBack: Boolean by Delegates.observable(false) {
        _, old, new -> notifyObservers (old, new, { onNavigationStateChanged() })
    }

    /**
     * Navigation state, true if there's an history item to go forward to, otherwise false.
     */
    var canGoForward: Boolean by Delegates.observable(false) {
        _, old, new -> notifyObservers (old, new, { onNavigationStateChanged() })
    }

    /**
     * Registers an observer that gets notified when the session changes.
     */
    fun register(observer: Observer) = synchronized(observers) {
        observers.add(observer)
    }

    /**
     * Unregisters an observer.
     */
    fun unregister(observer: Observer) = synchronized(observers) {
        observers.remove(observer)
    }

    /**
     * Helper method to notify observers.
     */

    private fun notifyObservers(old: Any, new: Any, block: Observer.() -> Unit) = synchronized(observers) {
        if (old != new) {
            notifyObservers(block)
        }
    }

    internal fun notifyObservers(block: Observer.() -> Unit) = synchronized(observers) {
        observers.forEach { it.block() }
    }
}
