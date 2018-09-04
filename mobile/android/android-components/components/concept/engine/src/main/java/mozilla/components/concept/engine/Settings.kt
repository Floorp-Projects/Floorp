/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.engine

import mozilla.components.concept.engine.EngineSession.TrackingProtectionPolicy
import mozilla.components.concept.engine.request.RequestInterceptor

/**
 * Holds settings of an engine or session. Concrete engine
 * implementations define how these settings are applied i.e.
 * whether a setting is applied on an engine or session instance.
 */
interface Settings {
    /**
     * Setting to control whether or not JavaScript is enabled.
     */
    var javascriptEnabled: Boolean
        get() = throw UnsupportedSettingException()
        set(_) = throw UnsupportedSettingException()

    /**
     * Setting to control whether or not DOM Storage is enabled.
     */
    var domStorageEnabled: Boolean
        get() = throw UnsupportedSettingException()
        set(_) = throw UnsupportedSettingException()

    /**
     * Setting to control whether or not Web fonts are enabled.
     */
    var webFontsEnabled: Boolean
        get() = throw UnsupportedSettingException()
        set(_) = throw UnsupportedSettingException()

    /**
     * Setting to control tracking protection.
     */
    var trackingProtectionPolicy: TrackingProtectionPolicy?
        get() = throw UnsupportedSettingException()
        set(_) = throw UnsupportedSettingException()

    /**
     * Setting to intercept and override requests.
     */
    var requestInterceptor: RequestInterceptor?
        get() = throw UnsupportedSettingException()
        set(_) = throw UnsupportedSettingException()
}

/**
 * [Settings] implementation used to set defaults for [Engine] and [EngineSession].
 */
data class DefaultSettings(
    override var javascriptEnabled: Boolean = true,
    override var domStorageEnabled: Boolean = true,
    override var webFontsEnabled: Boolean = true,
    override var trackingProtectionPolicy: TrackingProtectionPolicy? = null,
    override var requestInterceptor: RequestInterceptor? = null
) : Settings

/**
 * Exception thrown by default if a setting is not supported by an engine or session.
 */
class UnsupportedSettingException : RuntimeException("Setting not supported by this engine")
