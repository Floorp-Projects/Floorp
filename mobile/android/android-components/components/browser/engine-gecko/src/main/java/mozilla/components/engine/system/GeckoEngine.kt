/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.engine.system

import android.content.Context
import android.util.AttributeSet
import mozilla.components.engine.Engine
import mozilla.components.engine.EngineSession
import mozilla.components.engine.EngineView
import org.mozilla.geckoview.GeckoRuntime

/**
 * Gecko-based implementation of Engine interface.
 */
class GeckoEngine(
    private val runtime: GeckoRuntime
) : Engine {

    /**
     * Create a new Gecko-based EngineView.
     */
    override fun createView(context: Context, attrs: AttributeSet?): EngineView {
        return GeckoEngineView(context, attrs)
    }

    /**
     * Create a new Gecko-based EngineSession.
     */
    override fun createSession(): EngineSession {
        return GeckoEngineSession(runtime)
    }
}
