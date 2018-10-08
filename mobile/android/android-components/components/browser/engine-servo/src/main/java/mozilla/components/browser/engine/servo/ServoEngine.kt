/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.servo

import android.content.Context
import android.util.AttributeSet
import mozilla.components.concept.engine.DefaultSettings
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.EngineView
import mozilla.components.concept.engine.Settings

/**
 * Servo-based implementation of the Engine interface.
 */
class ServoEngine(
    private val defaultSettings: DefaultSettings? = DefaultSettings()
) : Engine {
    override fun createView(context: Context, attrs: AttributeSet?): EngineView {
        return ServoEngineView(context, attrs)
    }

    override fun createSession(private: Boolean): EngineSession {
        return ServoEngineSession(defaultSettings)
    }

    override fun name(): String = "Servo"

    /**
     * See [Engine.settings]
     */
    override val settings: Settings
        get() = throw UnsupportedOperationException("""Not supported by this implementation:
            Use EngineSession.settings instead""".trimIndent())
}
