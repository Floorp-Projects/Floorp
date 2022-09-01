/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ext

import android.net.Uri

/**
 * Returns true if the url contains any query parameters specified by the [searchParameters].
 *
 * @param searchParameters [String] of the following forms:
 * - "" (empty) - Don't search for any params
 * - "key" - Search param named "key" with any or no value
 * - "key=" - Search param named "key" with no value
 * - "key=value" - Search param named "key" with value "value"
 */
fun Uri.containsQueryParameters(searchParameters: String): Boolean {
    if (searchParameters.isBlank() || this.isOpaque) {
        return false
    }
    val params = searchParameters.split("=")
    return when (params.size) {
        1 -> {
            this.queryParameterNames.contains(params.first()) &&
                this.getQueryParameter(params.first()).isNullOrBlank()
        }
        2 -> {
            this.queryParameterNames.contains(params.first()) &&
                this.getQueryParameter(params.first()) == params.last()
        }
        else -> false
    }
}
