/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.gecko

import android.content.Context
import android.util.AttributeSet
import mozilla.components.browser.engine.gecko.integration.LocaleSettingUpdater
import mozilla.components.browser.engine.gecko.mediaquery.from
import mozilla.components.browser.engine.gecko.mediaquery.toGeckoValue
import mozilla.components.browser.engine.gecko.webextension.GeckoWebExtension
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.EngineSession.TrackingProtectionPolicy
import mozilla.components.concept.engine.EngineSessionState
import mozilla.components.concept.engine.EngineView
import mozilla.components.concept.engine.Settings
import mozilla.components.concept.engine.history.HistoryTrackingDelegate
import mozilla.components.concept.engine.mediaquery.PreferredColorScheme
import mozilla.components.concept.engine.webextension.WebExtension
import org.json.JSONObject
import org.mozilla.geckoview.GeckoResult
import org.mozilla.geckoview.GeckoRuntime
import org.mozilla.geckoview.GeckoRuntimeSettings
import org.mozilla.geckoview.GeckoSession
import org.mozilla.geckoview.GeckoWebExecutor

/**
 * Gecko-based implementation of Engine interface.
 */
class GeckoEngine(
    context: Context,
    private val defaultSettings: Settings? = null,
    private val runtime: GeckoRuntime = GeckoRuntime.getDefault(context),
    executorProvider: () -> GeckoWebExecutor = { GeckoWebExecutor(runtime) }
) : Engine {
    private val executor by lazy { executorProvider.invoke() }

    private val localeUpdater = LocaleSettingUpdater(context, runtime)

    init {
        runtime.delegate = GeckoRuntime.Delegate {
            // On shutdown: The runtime is shutting down (possibly because of an unrecoverable error state). We crash
            // the app here for two reasons:
            //  - We want to know about those unsolicited shutdowns and fix those issues.
            //  - We can't recover easily from this situation. Just continuing will leave us with an engine that
            //    doesn't do anything anymore.
            @Suppress("TooGenericExceptionThrown")
            throw RuntimeException("GeckoRuntime is shutting down")
        }
    }

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

    /**
     * See [Engine.createSessionState].
     */
    override fun createSessionState(json: JSONObject): EngineSessionState {
        return GeckoEngineSessionState.fromJSON(json)
    }

    /**
     * Opens a speculative connection to the host of [url].
     *
     * This is useful if an app thinks it may be making a request to that host in the near future. If no request
     * is made, the connection will be cleaned up after an unspecified.
     */
    override fun speculativeConnect(url: String) {
        executor.speculativeConnect(url)
    }

    /**
     * See [Engine.installWebExtension].
     */
    override fun installWebExtension(
        id: String,
        url: String,
        allowContentMessaging: Boolean,
        onSuccess: ((WebExtension) -> Unit),
        onError: ((String, Throwable) -> Unit)
    ) {
        GeckoWebExtension(id, url, allowContentMessaging).also { ext ->
            runtime.registerWebExtension(ext.nativeExtension).then({
                onSuccess(ext)
                GeckoResult<Void>()
            }, {
                throwable -> onError(id, throwable)
                GeckoResult<Void>()
            })
        }
    }

    override fun name(): String = "Gecko"

    /**
     * See [Engine.settings]
     */
    override val settings: Settings = object : Settings() {
        override var javascriptEnabled: Boolean
            get() = runtime.settings.javaScriptEnabled
            set(value) { runtime.settings.javaScriptEnabled = value }

        override var webFontsEnabled: Boolean
            get() = runtime.settings.webFontsEnabled
            set(value) { runtime.settings.webFontsEnabled = value }

        override var automaticFontSizeAdjustment: Boolean
            get() = runtime.settings.automaticFontSizeAdjustment
            set(value) { runtime.settings.automaticFontSizeAdjustment = value }

        override var automaticLanguageAdjustment: Boolean
            get() = localeUpdater.enabled
            set(value) {
                localeUpdater.enabled = value
                defaultSettings?.automaticLanguageAdjustment = value
            }

        override var trackingProtectionPolicy: TrackingProtectionPolicy?
            get() = TrackingProtectionPolicy.select(runtime.settings.contentBlocking.categories)
            set(value) {
                value?.let {
                    runtime.settings.contentBlocking.categories = it.categories
                    defaultSettings?.trackingProtectionPolicy = value
                }
            }

        override var remoteDebuggingEnabled: Boolean
            get() = runtime.settings.remoteDebuggingEnabled
            set(value) { runtime.settings.remoteDebuggingEnabled = value }

        override var historyTrackingDelegate: HistoryTrackingDelegate?
            get() = defaultSettings?.historyTrackingDelegate
            set(value) { defaultSettings?.historyTrackingDelegate = value }

        override var testingModeEnabled: Boolean
            get() = defaultSettings?.testingModeEnabled ?: false
            set(value) { defaultSettings?.testingModeEnabled = value }

        override var userAgentString: String?
            get() = defaultSettings?.userAgentString ?: GeckoSession.getDefaultUserAgent()
            set(value) { defaultSettings?.userAgentString = value }

        override var preferredColorScheme: PreferredColorScheme
            get() = PreferredColorScheme.from(runtime.settings.preferredColorScheme)
            set(value) { runtime.settings.preferredColorScheme = value.toGeckoValue() }

        override var allowAutoplayMedia: Boolean
            get() = runtime.settings.autoplayDefault == GeckoRuntimeSettings.AUTOPLAY_DEFAULT_ALLOWED
            set(value) {
                runtime.settings.autoplayDefault = if (value) {
                    GeckoRuntimeSettings.AUTOPLAY_DEFAULT_ALLOWED
                } else {
                    GeckoRuntimeSettings.AUTOPLAY_DEFAULT_BLOCKED
                }
            }

        override var suspendMediaWhenInactive: Boolean
            get() = defaultSettings?.suspendMediaWhenInactive ?: false
            set(value) { defaultSettings?.suspendMediaWhenInactive = value }

        override var fontInflationEnabled: Boolean
            get() = runtime.settings.fontInflationEnabled
            set(value) {
                // Setting, even to the default value, causes an Exception if
                // automaticFontSizeAdjustment is set to true (its default).
                // So, let's only set if the value has changed.
                if (value != runtime.settings.fontInflationEnabled) {
                    runtime.settings.fontInflationEnabled = value
                }
            }

        override var fontSizeFactor: Float
            get() = runtime.settings.fontSizeFactor
            set(value) {
                // Setting, even to the default value, causes an Exception if
                // automaticFontSizeAdjustment is set to true (its default).
                // So, let's only set if the value has changed.
                if (value != runtime.settings.fontSizeFactor) {
                    runtime.settings.fontSizeFactor = value
                }
            }
    }.apply {
        defaultSettings?.let {
            this.javascriptEnabled = it.javascriptEnabled
            this.webFontsEnabled = it.webFontsEnabled
            this.automaticFontSizeAdjustment = it.automaticFontSizeAdjustment
            this.automaticLanguageAdjustment = it.automaticLanguageAdjustment
            this.trackingProtectionPolicy = it.trackingProtectionPolicy
            this.remoteDebuggingEnabled = it.remoteDebuggingEnabled
            this.testingModeEnabled = it.testingModeEnabled
            this.userAgentString = it.userAgentString
            this.preferredColorScheme = it.preferredColorScheme
            this.allowAutoplayMedia = it.allowAutoplayMedia
            this.suspendMediaWhenInactive = it.suspendMediaWhenInactive
            this.fontInflationEnabled = it.fontInflationEnabled
            this.fontSizeFactor = it.fontSizeFactor
        }
    }
}
