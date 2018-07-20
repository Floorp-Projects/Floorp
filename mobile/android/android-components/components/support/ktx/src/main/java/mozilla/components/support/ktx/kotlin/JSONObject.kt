/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.ktx.kotlin

import org.json.JSONObject
import java.util.TreeMap

/**
 * Returns the value mapped by {@code key} if it exists, and
 * if the value returned is not null. If it's null, it returns null
 */
fun JSONObject.tryGetString(key: String): String? {
    if (!isNull(key)) {
        return getString(key)
    }
    return null
}

/**
 * Returns the value mapped by {@code key} if it exists, and
 * if the value returned is not null. If it's null, it returns null
 */
fun JSONObject.tryGetInt(key: String): Int? {
    if (!isNull(key)) {
        return getInt(key)
    }
    return null
}

/**
 * Returns the value mapped by {@code key} if it exists, and
 * if the value returned is not null. If it's null, it returns null
 */
fun JSONObject.tryGetLong(key: String): Long? {
    if (!isNull(key)) {
        return getLong(key)
    }
    return null
}

/**
 * Puts the specified value under the key if it's not null
 */
fun JSONObject.putIfNotNull(key: String, value: Any?) {
    if (value != null) {
        put(key, value)
    }
}

/**
 * Sorts the keys of a JSONObject (and all of its child JSONObjects) alphabetically
 */
fun JSONObject.sortKeys(): JSONObject {
    val map = TreeMap<String, Any>()
    for (key in this.keys()) {
        map[key] = this[key]
    }
    val jsonObject = JSONObject()
    for (key in map.keys) {
        if (map[key] is JSONObject) {
            map[key] = (map[key] as JSONObject).sortKeys()
        }
        jsonObject.put(key, map[key])
    }
    return jsonObject
}
