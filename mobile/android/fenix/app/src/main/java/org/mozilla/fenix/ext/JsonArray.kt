package org.mozilla.fenix.ext

import org.json.JSONArray

@Suppress("UNCHECKED_CAST")
fun <T> JSONArray.toList(): List<T> {
    val result = ArrayList<T>()
    for (i in 0 until length()) {
        result.add(get(i) as T)
    }
    return result
}
