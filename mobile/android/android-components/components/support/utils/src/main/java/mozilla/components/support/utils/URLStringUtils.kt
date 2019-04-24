/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.utils

import android.net.Uri
import android.text.TextUtils

object URLStringUtils {
    fun isURLLike(string: String, safe: Boolean = false) =
        if (safe) {
            string.matches(WebURLFinder.fuzzyUrlRegex)
        } else {
            string.matches(WebURLFinder.fuzzyUrlNonWebRegex)
        }

    fun isSearchTerm(string: String) = !isURLLike(string, false)

    /**
     * Normalizes a URL String.
     */
    fun toNormalizedURL(string: String): String {
        val trimmedInput = string.trim()
        var uri = Uri.parse(trimmedInput)
        if (TextUtils.isEmpty(uri.scheme)) {
            uri = Uri.parse("http://$trimmedInput")
        }
        return uri.toString()
    }
}
