/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.ktx.android.net

import android.net.Uri

private val commonPrefixes = listOf("www.", "mobile.", "m.")

/**
 * Returns the host without common prefixes like "www" or "m".
 */
val Uri.hostWithoutCommonPrefixes: String?
    get() {
        val host = host ?: return null
        for (prefix in commonPrefixes) {
            if (host.startsWith(prefix)) return host.substring(prefix.length)
        }
        return host
    }

/**
 * Returns true if the [Uri] uses the "http" or "https" protocol scheme.
 */
val Uri.isHttpOrHttps: Boolean
    get() = scheme == "http" || scheme == "https"
