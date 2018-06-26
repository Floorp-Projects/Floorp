/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.system

import android.content.Context
import android.util.AttributeSet
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.EngineView

/**
 * WebView-based implementation of the Engine interface.
 */
class SystemEngine : Engine {

    /**
     * Createa a new WebView-based EngineView implementation.
     */
    override fun createView(context: Context, attrs: AttributeSet?): EngineView {
        return SystemEngineView(context, attrs)
    }

    /**
     * Createa a new WebView-based EngineSession implementation.
     */
    override fun createSession(): EngineSession {
        return SystemEngineSession()
    }

    /**
     * See [Engine.name]
     */
    override fun name(): String = "System"
}
