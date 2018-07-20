/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.ktx.kotlin

import org.json.JSONArray

/**
 * Converts the list into a JSON array
 */
fun <T> List<T>.toJsonArray(): JSONArray {
    return fold(JSONArray()) { jsonArray, element -> jsonArray.put(element) }
}
