/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.fretboard

import org.json.JSONArray
import org.json.JSONObject

fun <T> List<T>.toJsonArray(): JSONArray {
    return fold(JSONArray()) { jsonArray, element -> jsonArray.put(element) }
}

@Suppress("UNCHECKED_CAST")
fun <T> JSONArray?.toList(): List<T> {
    if (this != null) {
        val result = ArrayList<T>()
        for (i in 0 until length())
            result.add(get(i) as T)
        return result
    }
    return listOf()
}

fun JSONObject.tryGetString(key: String): String? {
    if (!isNull(key)) {
        return getString(key)
    }
    return null
}

fun JSONObject.tryGetInt(key: String): Int? {
    if (!isNull(key)) {
        return getInt(key)
    }
    return null
}

fun JSONObject.tryGetLong(key: String): Long? {
    if (!isNull(key)) {
        return getLong(key)
    }
    return null
}

fun JSONObject.putIfNotNull(key: String, value: Any?) {
    if (value != null) {
        put(key, value)
    }
}
