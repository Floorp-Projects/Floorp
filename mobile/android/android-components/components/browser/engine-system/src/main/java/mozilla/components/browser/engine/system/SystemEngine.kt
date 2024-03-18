/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.system

import android.content.Context
import android.util.AttributeSet
import android.util.JsonReader
import android.webkit.WebSettings
import android.webkit.WebView
import androidx.annotation.VisibleForTesting
import mozilla.components.concept.base.profiler.Profiler
import mozilla.components.concept.engine.DefaultSettings
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.EngineSession.TrackingProtectionPolicy
import mozilla.components.concept.engine.EngineSessionState
import mozilla.components.concept.engine.EngineView
import mozilla.components.concept.engine.Settings
import mozilla.components.concept.engine.history.HistoryTrackingDelegate
import mozilla.components.concept.engine.utils.EngineVersion
import org.json.JSONObject
import java.lang.IllegalStateException

/**
 * WebView-based implementation of the Engine interface.
 */
class SystemEngine(
    private val context: Context,
    private val defaultSettings: Settings = DefaultSettings(),
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
    override fun createSession(private: Boolean, contextId: String?): EngineSession {
        if (private) {
            // TODO Implement private browsing: https://github.com/mozilla-mobile/android-components/issues/649
            throw UnsupportedOperationException("Private browsing is not supported in ${this::class.java.simpleName}")
        } else if (contextId != null) {
            throw UnsupportedOperationException(
                "Contextual identities are not supported in ${this::class.java.simpleName}",
            )
        }

        return SystemEngineSession(context, defaultSettings)
    }

    /**
     * Opens a speculative connection to the host of [url].
     *
     * Note: This implementation is a no-op.
     */
    override fun speculativeConnect(url: String) = Unit

    /**
     * See [Engine.profiler].
     */
    override val profiler: Profiler? = null

    /**
     * See [Engine.name]
     */
    override fun name(): String = "System"

    @Suppress("TooGenericExceptionCaught")
    override val version: EngineVersion
        get() {
            val userAgent = WebSettings.getDefaultUserAgent(context)
            val version = try {
                "Chrome/([^ ]+)".toRegex().find(userAgent)?.groups?.get(1)?.value
                    ?: throw IllegalStateException("Could not get version from user agent: $userAgent")
            } catch (e: IllegalStateException) {
                throw IllegalStateException("Could not get version from user agent: $userAgent")
            } catch (e: IndexOutOfBoundsException) {
                throw IllegalStateException("Could not get version from user agent: $userAgent")
            }

            return EngineVersion.parse(version)
                ?: throw IllegalStateException("Could not determine engine version: $version")
        }

    override fun createSessionState(json: JSONObject): EngineSessionState {
        return SystemEngineSessionState.fromJSON(json)
    }

    override fun createSessionStateFrom(reader: JsonReader): EngineSessionState {
        return SystemEngineSessionState.from(reader)
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

        override var historyTrackingDelegate: HistoryTrackingDelegate?
            get() = defaultSettings.historyTrackingDelegate
            set(value) {
                defaultSettings.historyTrackingDelegate = value
            }
    }.apply {
        this.remoteDebuggingEnabled = defaultSettings.remoteDebuggingEnabled
        this.trackingProtectionPolicy = defaultSettings.trackingProtectionPolicy
        if (defaultSettings.userAgentString == null) {
            defaultSettings.userAgentString = defaultUserAgent
        }
    }

    companion object {
        // In Robolectric tests we can't call WebSettings.getDefaultUserAgent(context)
        // as this would result in a NPE. So, we expose this field to circumvent the call.
        @VisibleForTesting
        var defaultUserAgent: String? = null

        private fun initDefaultUserAgent(context: Context): String {
            if (defaultUserAgent == null) {
                defaultUserAgent = WebSettings.getDefaultUserAgent(context)
            }
            return defaultUserAgent as String
        }
    }
}
