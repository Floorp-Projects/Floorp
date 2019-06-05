/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.system

import android.os.Bundle
import mozilla.components.concept.engine.EngineSessionState
import org.json.JSONObject

class SystemEngineSessionState(
    internal val bundle: Bundle?
) : EngineSessionState {
    override fun toJSON(): JSONObject {
        if (bundle == null) {
            return JSONObject()
        }

        return JSONObject().apply {
            bundle.keySet().forEach { key ->
                val value = bundle[key]

                if (shouldSerialize(value)) {
                    put(key, value)
                }
            }
        }
    }

    companion object {
        fun fromJSON(json: JSONObject): SystemEngineSessionState {
            return SystemEngineSessionState(json.toBundle())
        }
    }
}

private fun shouldSerialize(value: Any?): Boolean {
    // For now we only persist primitive types
    // https://github.com/mozilla-mobile/android-components/issues/279
    return when (value) {
        is Number -> true
        is Boolean -> true
        is String -> true
        else -> false
    }
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
