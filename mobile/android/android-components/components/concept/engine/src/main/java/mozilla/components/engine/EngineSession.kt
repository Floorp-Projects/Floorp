/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.engine

import android.support.annotation.CallSuper

/**
 * Class representing a single engine session.
 *
 * In browsers usually a session corresponds to a tab.
 */
abstract class EngineSession {
    /**
     * Interface to be implemented by classes that want to observer this engine session.
     */
    interface Observer {
        fun onLocationChange(url: String)
    }

    private val observers = mutableListOf<Observer>()

    /**
     * Register an observer that will be notified if this session changes (e.g. new location).
     */
    fun register(observer: Observer) {
        observers.add(observer)
    }

    /**
     * Unregister an observer.
     */
    fun unregister(observer: Observer) {
        observers.remove(observer)
    }

    /**
     * Helper method for notifying all observers.
     */
    protected fun notifyObservers(block: Observer.() -> Unit) {
        observers.forEach {
            it.block()
        }
    }

    /**
     * Load the given URL.
     */
    abstract fun loadUrl(url: String)

    /**
     * Close the session. This may free underlying objects. Call this when you are finished using
     * this session.
     */
    @CallSuper
    fun close() {
        observers.clear()
    }
}
