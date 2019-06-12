/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.servo

import android.content.Context
import android.util.AttributeSet
import mozilla.components.concept.engine.DefaultSettings
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.EngineSessionState
import mozilla.components.concept.engine.EngineView
import mozilla.components.concept.engine.Settings
import mozilla.components.concept.engine.history.HistoryTrackingDelegate
import mozilla.components.concept.engine.utils.EngineVersion
import org.json.JSONObject

/**
 * Servo-based implementation of the Engine interface.
 */
class ServoEngine(
    private val defaultSettings: DefaultSettings = DefaultSettings()
) : Engine {
    override fun createView(context: Context, attrs: AttributeSet?): EngineView {
        return ServoEngineView(context, attrs)
    }

    override fun createSession(private: Boolean): EngineSession {
        return ServoEngineSession(defaultSettings)
    }

    override fun createSessionState(json: JSONObject): EngineSessionState {
        return ServoEngineSessionState.fromJSON(json)
    }

    /**
     * Opens a speculative connection to the host of [url].
     *
     * Note: This implementation is a no-op.
     */
    override fun speculativeConnect(url: String) = Unit

    override fun name(): String = "Servo"

    override val version: EngineVersion = EngineVersion(0, 0, 0)

    /**
     * See [Engine.settings]
     */
    override val settings: Settings = object : Settings() {
        // History tracking is not supported/implemented currently, but we want
        // to be able to start up with the history tracking feature enabled.
        override var historyTrackingDelegate: HistoryTrackingDelegate?
            get() = defaultSettings.historyTrackingDelegate
            set(value) {
                defaultSettings.historyTrackingDelegate = value
            }
    }
}
