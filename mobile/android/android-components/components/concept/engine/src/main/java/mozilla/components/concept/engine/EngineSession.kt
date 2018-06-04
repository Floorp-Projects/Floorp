/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.engine

import android.support.annotation.CallSuper

/**
 * Class representing a single engine session.
 *
 * In browsers usually a session corresponds to a tab.
 */
abstract class EngineSession {
    /**
     * Interface to be implemented by classes that want to observe this engine session.
     */
    interface Observer {
        fun onLocationChange(url: String)
        fun onProgress(progress: Int)
        fun onLoadingStateChange(loading: Boolean)
        fun onNavigationStateChange(canGoBack: Boolean? = null, canGoForward: Boolean? = null)
        fun onSecurityChange(secure: Boolean, host: String? = null, issuer: String? = null)
    }

    private val observers = mutableListOf<Observer>()

    /**
     * Register an observer that will be notified if this session changes (e.g. new location).
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
     * Helper method for notifying all observers.
     */
    protected fun notifyObservers(block: Observer.() -> Unit) = synchronized(observers) {
        observers.forEach {
            it.block()
        }
    }

    /**
     * Loads the given URL.
     */
    abstract fun loadUrl(url: String)

    /**
     * Reloads the current URL.
     */
    abstract fun reload()

    /**
     * Navigates back in the history of this session.
     */
    abstract fun goBack()

    /**
     * Navigates forward in the history of this session.
     */
    abstract fun goForward()

    /**
     * Saves and returns the engine state. Engine implementations are not required
     * to persist the state anywhere else than in the returned map. Engines that
     * already provide a serialized state can use a single entry in this map to
     * provide this state. The only requirement is that the same map can be used
     * to restore the original state. See [restoreState] and the specific
     * engine implementation for details.
     */
    abstract fun saveState(): Map<String, Any>

    /**
     * Restores the engine state as provided by [saveState].
     *
     * @param state state retrieved from [saveState]
     */
    abstract fun restoreState(state: Map<String, Any>)

    /**
     * Close the session. This may free underlying objects. Call this when you are finished using
     * this session.
     */
    @CallSuper
    fun close() = synchronized(observers) {
        observers.clear()
    }
}
