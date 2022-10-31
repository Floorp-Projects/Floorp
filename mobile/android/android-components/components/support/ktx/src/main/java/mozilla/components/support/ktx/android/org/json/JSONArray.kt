/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.ktx.android.org.json

import org.json.JSONArray
import org.json.JSONException

/**
 * Convenience method to convert a JSONArray into a sequence.
 *
 * @param getter callback to get the value for an index in the array.
 */
inline fun <V> JSONArray.asSequence(crossinline getter: JSONArray.(Int) -> V): Sequence<V> {
    val indexRange = 0 until length()
    return indexRange.asSequence().map { i -> getter(i) }
}

fun JSONArray.asSequence(): Sequence<Any> = asSequence { i -> get(i) }

/**
 * Convenience method to convert a JSONArray into a List
 *
 * @return list with the JSONArray values, or an empty list if the JSONArray was null
 */
@Suppress("UNCHECKED_CAST")
fun <T> JSONArray?.toList(): List<T> {
    val array = this ?: return emptyList()
    return array.asSequence().map { it as T }.toList()
}

/**
 * Returns a list containing only the non-null results of applying the given [transform] function
 * to each element in the original collection as returned by [getFromArray]. If [getFromArray]
 * or [transform] throws a [JSONException], these elements will also be omitted.
 *
 * Here's an example call:
 * ```kotlin
 * jsonArray.mapNotNull(JSONArray::getJSONObject) { jsonObj -> jsonObj.getString("author") }
 * ```
 */
inline fun <T, R : Any> JSONArray.mapNotNull(getFromArray: JSONArray.(index: Int) -> T, transform: (T) -> R?): List<R> {
    val transformedResults = mutableListOf<R>()
    for (i in 0 until this.length()) {
        try {
            val transformed = transform(getFromArray(i))
            if (transformed != null) { transformedResults.add(transformed) }
        } catch (e: JSONException) { /* Do nothing: we skip bad data. */ }
    }

    return transformedResults
}

fun Iterable<Any>.toJSONArray() = JSONArray().also { array ->
    forEach { array.put(it) }
}
