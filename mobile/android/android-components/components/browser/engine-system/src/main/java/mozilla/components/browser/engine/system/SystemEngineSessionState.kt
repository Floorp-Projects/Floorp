/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.system

import android.os.Bundle
import android.util.JsonReader
import android.util.JsonToken
import android.util.JsonWriter
import mozilla.components.concept.engine.EngineSessionState
import org.json.JSONObject

class SystemEngineSessionState(
    internal val bundle: Bundle?,
) : EngineSessionState {
    override fun writeTo(writer: JsonWriter) {
        writer.beginObject()

        bundle?.keySet()?.forEach { key ->
            when (val value = bundle[key]) {
                is Number -> writer.name(key).value(value)
                is String -> writer.name(key).value(value)
                is Boolean -> writer.name(key).value(value)
            }
        }

        writer.endObject()
        writer.flush()
    }

    companion object {
        fun fromJSON(json: JSONObject): SystemEngineSessionState {
            return SystemEngineSessionState(json.toBundle())
        }

        /**
         * Creates a [SystemEngineSessionState] from the given [JsonReader].
         */
        fun from(reader: JsonReader): SystemEngineSessionState {
            return SystemEngineSessionState(reader.toBundle())
        }
    }
}

private fun JsonReader.toBundle(): Bundle {
    beginObject()

    val bundle = Bundle()

    while (peek() != JsonToken.END_OBJECT) {
        val name = nextName()

        when (peek()) {
            JsonToken.NULL -> nextNull()
            JsonToken.BOOLEAN -> bundle.putBoolean(name, nextBoolean())
            JsonToken.STRING -> bundle.putString(name, nextString())
            JsonToken.NUMBER -> bundle.putDouble(name, nextDouble())
            JsonToken.BEGIN_OBJECT -> bundle.putBundle(name, toBundle())
            else -> skipValue()
        }
    }

    endObject()

    return bundle
}

private fun JSONObject.toBundle(): Bundle {
    val bundle = Bundle()

    keys().forEach { key ->
        val value = get(key)
        bundle.put(key, value)
    }

    return bundle
}

private fun Bundle.put(key: String, value: Any) {
    when (value) {
        is Int -> putInt(key, value)
        is Double -> putDouble(key, value)
        is Long -> putLong(key, value)
        is Float -> putFloat(key, value)
        is Char -> putChar(key, value)
        is Short -> putShort(key, value)
        is Byte -> putByte(key, value)
        is String -> putString(key, value)
        is Boolean -> putBoolean(key, value)
    }
}
