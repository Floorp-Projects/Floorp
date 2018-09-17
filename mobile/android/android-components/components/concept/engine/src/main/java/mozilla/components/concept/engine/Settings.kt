/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.engine

import mozilla.components.concept.engine.EngineSession.TrackingProtectionPolicy
import mozilla.components.concept.engine.request.RequestInterceptor
import kotlin.reflect.KProperty

/**
 * Holds settings of an engine or session. Concrete engine
 * implementations define how these settings are applied i.e.
 * whether a setting is applied on an engine or session instance.
 */
@Suppress("UnnecessaryAbstractClass")
abstract class Settings {
    /**
     * Setting to control whether or not JavaScript is enabled.
     */
    open var javascriptEnabled: Boolean by UnsupportedSetting()

    /**
     * Setting to control whether or not DOM Storage is enabled.
     */
    open var domStorageEnabled: Boolean by UnsupportedSetting()

    /**
     * Setting to control whether or not Web fonts are enabled.
     */
    open var webFontsEnabled: Boolean by UnsupportedSetting()

    /**
     * Setting to control tracking protection.
     */
    open var trackingProtectionPolicy: TrackingProtectionPolicy? by UnsupportedSetting()

    /**
     * Setting to intercept and override requests.
     */
    open var requestInterceptor: RequestInterceptor? by UnsupportedSetting()

    /**
     * Setting to control the user agent string.
     */
    open var userAgentString: String? by UnsupportedSetting()

    /**
     * Setting to control whether or not a user gesture is required to play media.
     */
    open var mediaPlaybackRequiresUserGesture: Boolean by UnsupportedSetting()

    /**
     * Setting to control whether or not window.open can be called from JavaScript.
     */
    open var javaScriptCanOpenWindowsAutomatically: Boolean by UnsupportedSetting()

    /**
     * Setting to control whether or not zoom controls should be displayed.
     */
    open var displayZoomControls: Boolean by UnsupportedSetting()

    /**
     * Setting to control whether or not the engine zooms out the content to fit on screen by width.
     */
    open var loadWithOverviewMode: Boolean by UnsupportedSetting()

    /**
     * Setting to control whether or not file access is allowed.
     */
    open var allowFileAccess: Boolean by UnsupportedSetting()

    /**
     * Setting to control whether or not JavaScript running in the context of a file scheme URL
     * should be allowed to access content from other file scheme URLs.
     */
    open var allowFileAccessFromFileURLs: Boolean by UnsupportedSetting()

    /**
     * Setting to control whether or not JavaScript running in the context of a file scheme URL
     * should be allowed to access content from any origin.
     */
    open var allowUniversalAccessFromFileURLs: Boolean by UnsupportedSetting()

    /**
     * Setting to control whether or not the engine is allowed to load content from a content
     * provider installed in the system.
     */
    open var allowContentAccess: Boolean by UnsupportedSetting()

    /**
     * Setting to control whether or not vertical scrolling is enabled.
     */
    open var verticalScrollBarEnabled: Boolean by UnsupportedSetting()

    /**
     * Setting to control whether or not horizontal scrolling is enabled.
     */
    open var horizontalScrollBarEnabled: Boolean by UnsupportedSetting()
}

/**
 * [Settings] implementation used to set defaults for [Engine] and [EngineSession].
 */
data class DefaultSettings(
    override var javascriptEnabled: Boolean = true,
    override var domStorageEnabled: Boolean = true,
    override var webFontsEnabled: Boolean = true,
    override var mediaPlaybackRequiresUserGesture: Boolean = true,
    override var trackingProtectionPolicy: TrackingProtectionPolicy? = null,
    override var requestInterceptor: RequestInterceptor? = null,
    override var userAgentString: String? = null,
    override var javaScriptCanOpenWindowsAutomatically: Boolean = false,
    override var displayZoomControls: Boolean = true,
    override var loadWithOverviewMode: Boolean = false,
    override var allowFileAccess: Boolean = true,
    override var allowFileAccessFromFileURLs: Boolean = false,
    override var allowUniversalAccessFromFileURLs: Boolean = false,
    override var allowContentAccess: Boolean = true,
    override var verticalScrollBarEnabled: Boolean = true,
    override var horizontalScrollBarEnabled: Boolean = true
) : Settings()

class UnsupportedSetting<T> {
    operator fun getValue(thisRef: Any?, prop: KProperty<*>): T {
        throw UnsupportedSettingException("Setting ${prop.name} is not supported by this engine!")
    }

    operator fun setValue(thisRef: Any?, prop: KProperty<*>, value: T) {
        throw UnsupportedSettingException("Setting ${prop.name} is not supported by this engine!")
    }
}

/**
 * Exception thrown by default if a setting is not supported by an engine or session.
 */
class UnsupportedSettingException(message: String = "Setting not supported by this engine") : RuntimeException(message)
