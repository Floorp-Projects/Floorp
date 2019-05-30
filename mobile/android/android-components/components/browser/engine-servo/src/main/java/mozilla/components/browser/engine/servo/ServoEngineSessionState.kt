/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.servo

import mozilla.components.concept.engine.EngineSessionState
import org.json.JSONObject

/**
 * No-op implementation of [EngineSessionState].
 */
class ServoEngineSessionState : EngineSessionState {
    override fun toJSON() = JSONObject()

    override fun equals(other: Any?) =
        other != null && this::class == other::class

    override fun hashCode() = this::class.hashCode()

    companion object {
        @Suppress("UNUSED_PARAMETER")
        fun fromJSON(json: JSONObject) = ServoEngineSessionState()
    }
}
