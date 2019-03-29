/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.gecko

import mozilla.components.concept.engine.EngineSessionState
import org.json.JSONException
import org.json.JSONObject
import org.mozilla.geckoview.GeckoSession

private const val GECKO_STATE_KEY = "GECKO_STATE"

class GeckoEngineSessionState internal constructor(
    internal val actualState: GeckoSession.SessionState?
) : EngineSessionState {
    override fun toJSON() = JSONObject().apply {
        if (actualState != null) {
            // GeckoView provides a String representing the entire session state. We
            // store this String using a single Map entry with key GECKO_STATE_KEY.

            put(GECKO_STATE_KEY, actualState.toString())
        }
    }

    companion object {
        fun fromJSON(json: JSONObject): GeckoEngineSessionState = try {
            val state = json.getString(GECKO_STATE_KEY)

            GeckoEngineSessionState(
                GeckoSession.SessionState.fromString(state)
            )
        } catch (e: JSONException) {
            GeckoEngineSessionState(null)
        }
    }
}
