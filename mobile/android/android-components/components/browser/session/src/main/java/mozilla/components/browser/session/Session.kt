/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.session

import mozilla.components.support.utils.observer.Observable
import mozilla.components.support.utils.observer.ObserverRegistry
import mozilla.components.browser.session.tab.CustomTabConfig
import java.util.UUID
import kotlin.properties.Delegates

/**
 * Value type that represents the state of a browser session. Changes can be observed.
 */
class Session(
    initialUrl: String,
    val id: String = UUID.randomUUID().toString()
) : Observable<Session.Observer> by registry {
    /**
     * Interface to be implemented by classes that want to observe a session.
     */
    interface Observer {
        fun onUrlChanged()
        fun onProgress()
        fun onLoadingStateChanged()
        fun onNavigationStateChanged()
        fun onSearch()
        fun onSecurityChanged()
        fun onCustomTabConfigChanged() { }
    }

    /**
     * A value type holding security information for a Session.
     *
     * @property secure true if the session is currently pointed to a URL with
     * a valid SSL certificate, otherwise false.
     * @property host domain for which the SSL certificate was issued.
     * @property issuer name of the certificate authority who issued the SSL certificate.
     */
    data class SecurityInfo(val secure: Boolean = false, val host: String = "", val issuer: String = "")

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
     * The currently / last used search terms.
     */
    var searchTerms: String by Delegates.observable("") {
        _, _, new -> notifyObservers ({ if (!new.isEmpty()) onSearch() })
    }

    /**
     * Security information indicating whether or not the current session is
     * for a secure URL, as well as the host and SSL certificate authority, if applicable.
     */
    var securityInfo: SecurityInfo by Delegates.observable(SecurityInfo()) {
        _, old, new -> notifyObservers (old, new, { onSecurityChanged() })
    }

    /**
     * Configuration data in case this session is used for a Custom Tab.
     */
    var customTabConfig: CustomTabConfig? by Delegates.observable<CustomTabConfig?>(null) {
        _, _, _ -> notifyObservers ({ onCustomTabConfigChanged() })
    }

    /**
     * Helper method to notify observers.
     */
    private fun notifyObservers(old: Any, new: Any, block: Observer.() -> Unit) {
        if (old != new) {
            notifyObservers(block)
        }
    }

    override fun equals(other: Any?): Boolean {
        if (this === other) return true
        if (javaClass != other?.javaClass) return false

        other as Session
        if (id != other.id) return false
        return true
    }

    override fun hashCode(): Int {
        return id.hashCode()
    }
}

private val registry = ObserverRegistry<Session.Observer>()
