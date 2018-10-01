/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.system

import android.content.Context
import android.util.AttributeSet
import android.webkit.WebSettings
import android.webkit.WebView
import mozilla.components.concept.engine.DefaultSettings
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.EngineSession.TrackingProtectionPolicy
import mozilla.components.concept.engine.EngineView
import mozilla.components.concept.engine.Settings

/**
 * WebView-based implementation of the Engine interface.
 */
class SystemEngine(
    context: Context,
    private val defaultSettings: DefaultSettings = DefaultSettings()
) : Engine {

    init {
        initDefaultUserAgent(context)
    }
    /**
     * Creates a new WebView-based EngineView implementation.
     */
    override fun createView(context: Context, attrs: AttributeSet?): EngineView {
        return SystemEngineView(context, attrs)
    }

    /**
     * Creates a new WebView-based EngineSession implementation.
     */
    override fun createSession(private: Boolean): EngineSession {
        if (private) {
            // TODO Implement private browsing: https://github.com/mozilla-mobile/android-components/issues/649
            throw UnsupportedOperationException("Private browsing is not supported in ${this::class.java.simpleName}")
        }
        return SystemEngineSession(defaultSettings)
    }

    /**
     * See [Engine.name]
     */
    override fun name(): String = "System"

    /**
     * See [Engine.settings]
     */
    override val settings: Settings = object : Settings() {
        private var internalRemoteDebuggingEnabled = false
        override var remoteDebuggingEnabled: Boolean
            get() = internalRemoteDebuggingEnabled
            set(value) {
                WebView.setWebContentsDebuggingEnabled(value)
                internalRemoteDebuggingEnabled = value
            }

        override var userAgentString: String?
            get() = defaultSettings.userAgentString
            set(value) {
                defaultSettings.userAgentString = value
            }

        override var trackingProtectionPolicy: TrackingProtectionPolicy?
            get() = defaultSettings.trackingProtectionPolicy
            set(value) {
                defaultSettings.trackingProtectionPolicy = value
            }
    }.apply {
        this.remoteDebuggingEnabled = defaultSettings.remoteDebuggingEnabled
        this.trackingProtectionPolicy = defaultSettings.trackingProtectionPolicy
        if (defaultSettings.userAgentString == null) {
            defaultSettings.userAgentString = defaultUserAgent
        }
    }

    companion object {
        internal var defaultUserAgent: String? = null

        private fun initDefaultUserAgent(context: Context): String {
            if (defaultUserAgent == null) {
                defaultUserAgent = WebSettings.getDefaultUserAgent(context)
            }
            return defaultUserAgent as String
        }
    }
}
