/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.autocomplete

object AutocompleteDomainFormatter {
    private const val HOST_INDEX = 3
    private val urlMatcher = Regex("""(https?://)?(www.)?(.+)?""")

    fun format(url: String): String {
        val result = urlMatcher.find(url)

        return result?.let {
            it.groups[HOST_INDEX]?.value
        } ?: url
    }
}
