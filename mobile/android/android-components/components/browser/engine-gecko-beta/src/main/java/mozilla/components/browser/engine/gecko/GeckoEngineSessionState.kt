/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.gecko

import android.util.JsonReader
import android.util.JsonToken
import android.util.JsonWriter
import mozilla.components.concept.engine.EngineSessionState
import org.json.JSONException
import org.json.JSONObject
import org.mozilla.geckoview.GeckoSession
import java.io.IOException
import java.lang.IllegalStateException

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

    override fun writeTo(writer: JsonWriter) {
        with(writer) {
            beginObject()

            name(GECKO_STATE_KEY)
            value(actualState.toString())

            endObject()
            flush()
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

        /**
         * Creates a [GeckoEngineSessionState] from the given [JsonReader].
         */
        fun from(reader: JsonReader): GeckoEngineSessionState = try {
            reader.beginObject()

            val key = reader.nextName()

            val rawState = if (key == GECKO_STATE_KEY) {
                reader.nextString()
            } else {
                reader.consumeObject()
                null
            }

            reader.endObject()

            if (rawState != null) {
                GeckoEngineSessionState(GeckoSession.SessionState.fromString(rawState))
            } else {
                GeckoEngineSessionState(null)
            }
        } catch (e: IOException) {
            GeckoEngineSessionState(null)
        } catch (e: AssertionError) {
            GeckoEngineSessionState(null)
        } catch (e: IllegalStateException) {
            GeckoEngineSessionState(null)
        }
    }
}

private fun JsonReader.consumeObject() {
    while (true) {
        when (peek()) {
            JsonToken.END_OBJECT -> return
            else -> skipValue()
        }
    }
}
