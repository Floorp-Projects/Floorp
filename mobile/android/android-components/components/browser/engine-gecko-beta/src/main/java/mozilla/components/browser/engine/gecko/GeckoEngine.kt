/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.gecko

import android.content.Context
import android.util.AttributeSet
import mozilla.components.browser.errorpages.ErrorPages
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.EngineSession.TrackingProtectionPolicy
import mozilla.components.concept.engine.EngineView
import mozilla.components.concept.engine.Settings
import org.mozilla.geckoview.GeckoRuntime
import org.mozilla.geckoview.GeckoSession

/**
 * Gecko-based implementation of Engine interface.
 */
class GeckoEngine(
    context: Context,
    private val defaultSettings: Settings? = null,
    private val runtime: GeckoRuntime = GeckoRuntime.getDefault(context)
) : Engine {

    /**
     * Creates a new Gecko-based EngineView.
     */
    override fun createView(context: Context, attrs: AttributeSet?): EngineView {
        return GeckoEngineView(context, attrs)
    }

    /**
     * Creates a new Gecko-based EngineSession.
     */
    override fun createSession(private: Boolean): EngineSession {
        return GeckoEngineSession(runtime, private, defaultSettings)
    }

    override fun name(): String = "Gecko"

    /**
     * See [Engine.errorTypeFromCode]
     */
    override fun errorTypeFromCode(errorCode: Int, errorCategory: Int?): ErrorPages.ErrorType {
        if (errorCategory == null)
            throw IllegalArgumentException("No error type definition for error category $errorCategory")

        when (errorCategory) {
            GeckoSession.NavigationDelegate.ERROR_CATEGORY_UNKNOWN ->
                ErrorPages.ErrorType.UNKNOWN
            GeckoSession.NavigationDelegate.ERROR_CATEGORY_SECURITY -> {
                when (errorCode) {
                    GeckoSession.NavigationDelegate.ERROR_SECURITY_SSL ->
                        ErrorPages.ErrorType.ERROR_SECURITY_SSL
                    GeckoSession.NavigationDelegate.ERROR_SECURITY_BAD_CERT ->
                        ErrorPages.ErrorType.ERROR_SECURITY_BAD_CERT
                    else -> throw IllegalArgumentException("No error type definition for error code $errorCode")
                }
            }
            GeckoSession.NavigationDelegate.ERROR_CATEGORY_NETWORK -> {
                when (errorCode) {
                    GeckoSession.NavigationDelegate.ERROR_NET_INTERRUPT ->
                        ErrorPages.ErrorType.ERROR_NET_INTERRUPT
                    GeckoSession.NavigationDelegate.ERROR_NET_TIMEOUT ->
                        ErrorPages.ErrorType.ERROR_NET_TIMEOUT
                    GeckoSession.NavigationDelegate.ERROR_CONNECTION_REFUSED ->
                        ErrorPages.ErrorType.ERROR_CONNECTION_REFUSED
                    GeckoSession.NavigationDelegate.ERROR_UNKNOWN_SOCKET_TYPE ->
                        ErrorPages.ErrorType.ERROR_UNKNOWN_SOCKET_TYPE
                    GeckoSession.NavigationDelegate.ERROR_REDIRECT_LOOP ->
                        ErrorPages.ErrorType.ERROR_REDIRECT_LOOP
                    GeckoSession.NavigationDelegate.ERROR_OFFLINE ->
                        ErrorPages.ErrorType.ERROR_OFFLINE
                    GeckoSession.NavigationDelegate.ERROR_PORT_BLOCKED ->
                        ErrorPages.ErrorType.ERROR_PORT_BLOCKED
                    GeckoSession.NavigationDelegate.ERROR_NET_RESET ->
                        ErrorPages.ErrorType.ERROR_NET_RESET
                    else -> throw IllegalArgumentException("No error type definition for error code $errorCode")
                }
            }
            GeckoSession.NavigationDelegate.ERROR_CATEGORY_CONTENT -> {
                when (errorCode) {
                    GeckoSession.NavigationDelegate.ERROR_UNSAFE_CONTENT_TYPE ->
                        ErrorPages.ErrorType.ERROR_UNSAFE_CONTENT_TYPE
                    GeckoSession.NavigationDelegate.ERROR_CORRUPTED_CONTENT ->
                        ErrorPages.ErrorType.ERROR_CORRUPTED_CONTENT
                    GeckoSession.NavigationDelegate.ERROR_CONTENT_CRASHED ->
                        ErrorPages.ErrorType.ERROR_CONTENT_CRASHED
                    GeckoSession.NavigationDelegate.ERROR_INVALID_CONTENT_ENCODING ->
                        ErrorPages.ErrorType.ERROR_INVALID_CONTENT_ENCODING
                    else -> throw IllegalArgumentException("No error type definition for error code $errorCode")
                }
            }
            GeckoSession.NavigationDelegate.ERROR_CATEGORY_URI -> {
                when (errorCode) {
                    GeckoSession.NavigationDelegate.ERROR_UNKNOWN_HOST ->
                        ErrorPages.ErrorType.ERROR_UNKNOWN_HOST
                    GeckoSession.NavigationDelegate.ERROR_MALFORMED_URI ->
                        ErrorPages.ErrorType.ERROR_MALFORMED_URI
                    GeckoSession.NavigationDelegate.ERROR_UNKNOWN_PROTOCOL ->
                        ErrorPages.ErrorType.ERROR_UNKNOWN_PROTOCOL
                    GeckoSession.NavigationDelegate.ERROR_FILE_NOT_FOUND ->
                        ErrorPages.ErrorType.ERROR_FILE_NOT_FOUND
                    GeckoSession.NavigationDelegate.ERROR_FILE_ACCESS_DENIED ->
                        ErrorPages.ErrorType.ERROR_FILE_ACCESS_DENIED
                    else -> throw IllegalArgumentException("No error type definition for error code $errorCode")
                }
            }
            GeckoSession.NavigationDelegate.ERROR_CATEGORY_PROXY -> {
                when (errorCode) {
                    GeckoSession.NavigationDelegate.ERROR_PROXY_CONNECTION_REFUSED ->
                        ErrorPages.ErrorType.ERROR_PROXY_CONNECTION_REFUSED
                    GeckoSession.NavigationDelegate.ERROR_UNKNOWN_PROXY_HOST ->
                        ErrorPages.ErrorType.ERROR_UNKNOWN_PROXY_HOST
                    else -> throw IllegalArgumentException("No error type definition for error code $errorCode")
                }
            }
            GeckoSession.NavigationDelegate.ERROR_CATEGORY_SAFEBROWSING -> {
                when (errorCode) {
                    GeckoSession.NavigationDelegate.ERROR_SAFEBROWSING_MALWARE_URI ->
                        ErrorPages.ErrorType.ERROR_SAFEBROWSING_MALWARE_URI
                    GeckoSession.NavigationDelegate.ERROR_SAFEBROWSING_UNWANTED_URI ->
                        ErrorPages.ErrorType.ERROR_SAFEBROWSING_UNWANTED_URI
                    GeckoSession.NavigationDelegate.ERROR_SAFEBROWSING_HARMFUL_URI ->
                        ErrorPages.ErrorType.ERROR_SAFEBROWSING_HARMFUL_URI
                    GeckoSession.NavigationDelegate.ERROR_SAFEBROWSING_PHISHING_URI ->
                        ErrorPages.ErrorType.ERROR_SAFEBROWSING_PHISHING_URI
                    else -> throw IllegalArgumentException("No error type definition for error code $errorCode")
                }
            }
            else -> throw IllegalArgumentException("No error type definition for error category $errorCategory")
        }

        throw IllegalArgumentException("No error type definition for error category $errorCategory")
    }

    /**
     * See [Engine.settings]
     */
    override val settings: Settings = object : Settings() {
        override var javascriptEnabled: Boolean
            get() = runtime.settings.javaScriptEnabled
            set(value) {
                runtime.settings.javaScriptEnabled = value
            }

        override var webFontsEnabled: Boolean
            get() = runtime.settings.webFontsEnabled
            set(value) {
                runtime.settings.webFontsEnabled = value
            }

        override var trackingProtectionPolicy: TrackingProtectionPolicy?
            get() = TrackingProtectionPolicy.select(runtime.settings.trackingProtectionCategories)
            set(value) {
                value?.let {
                    runtime.settings.trackingProtectionCategories = it.categories
                }
            }

        override var remoteDebuggingEnabled: Boolean
            get() = runtime.settings.remoteDebuggingEnabled
            set(value) { runtime.settings.remoteDebuggingEnabled = value }
    }.apply {
        defaultSettings?.let {
            this.javascriptEnabled = it.javascriptEnabled
            this.webFontsEnabled = it.webFontsEnabled
            this.trackingProtectionPolicy = it.trackingProtectionPolicy
            this.remoteDebuggingEnabled = it.remoteDebuggingEnabled
        }
    }
}
