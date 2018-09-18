/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.system

import android.content.Context
import android.util.AttributeSet
import android.webkit.WebSettings
import android.webkit.WebView
import android.webkit.WebViewClient
import mozilla.components.browser.errorpages.ErrorPages
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
     * See [Engine.errorTypeFromCode]
     */
    override fun errorTypeFromCode(errorCode: Int, errorCategory: Int?): ErrorPages.ErrorType =
    // Chromium's mapping (internal error code, to Android WebView error code) is described at:
    // https://goo.gl/vspwct (ErrorCodeConversionHelper.java)
        when (errorCode) {
            WebViewClient.ERROR_UNKNOWN -> ErrorPages.ErrorType.UNKNOWN

            // This is probably the most commonly shown error. If there's no network, we inevitably
            // show this.
            WebViewClient.ERROR_HOST_LOOKUP -> ErrorPages.ErrorType.ERROR_UNKNOWN_HOST

            WebViewClient.ERROR_CONNECT -> ErrorPages.ErrorType.ERROR_CONNECTION_REFUSED

            // It's unclear what this actually means - it's not well documented. Based on looking at
            // ErrorCodeConversionHelper this could happen if networking is disabled during load, in which
            // case the generic error is good enough:
            WebViewClient.ERROR_IO -> ErrorPages.ErrorType.ERROR_CONNECTION_REFUSED

            WebViewClient.ERROR_TIMEOUT -> ErrorPages.ErrorType.ERROR_NET_TIMEOUT

            WebViewClient.ERROR_REDIRECT_LOOP -> ErrorPages.ErrorType.ERROR_REDIRECT_LOOP

            WebViewClient.ERROR_UNSUPPORTED_SCHEME -> ErrorPages.ErrorType.ERROR_UNKNOWN_PROTOCOL

            WebViewClient.ERROR_FAILED_SSL_HANDSHAKE -> ErrorPages.ErrorType.ERROR_SECURITY_SSL

            WebViewClient.ERROR_BAD_URL -> ErrorPages.ErrorType.ERROR_MALFORMED_URI

            // Seems to be an indication of OOM, insufficient resources, or too many queued DNS queries
            WebViewClient.ERROR_TOO_MANY_REQUESTS -> ErrorPages.ErrorType.UNKNOWN

            // There's no mapping for the following errors yet. At the time this library was
            // extracted from Focus we didn't use any of those errors.
            // WebViewClient.ERROR_UNSUPPORTED_AUTH_SCHEME
            // WebViewClient.ERROR_AUTHENTICATION
            // WebViewClient.ERROR_FILE
            // WebViewClient.ERROR_FILE_NOT_FOUND

            else -> throw IllegalArgumentException("No error type definition for error code $errorCode")
        }

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
