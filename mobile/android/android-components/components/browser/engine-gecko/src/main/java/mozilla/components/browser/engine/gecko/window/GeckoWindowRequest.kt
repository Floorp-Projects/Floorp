/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.gecko.window

import mozilla.components.browser.engine.gecko.GeckoEngineSession
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.window.WindowRequest

/**
 * Gecko-based implementation of [WindowRequest].
 */
class GeckoWindowRequest(
    override val url: String = "",
    private val engineSession: GeckoEngineSession,
    override val type: WindowRequest.Type = WindowRequest.Type.OPEN,
) : WindowRequest {

    override fun prepare(): EngineSession {
        return this.engineSession
    }
}
