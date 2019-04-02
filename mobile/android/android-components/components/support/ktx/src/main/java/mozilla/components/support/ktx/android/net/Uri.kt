/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.ktx.android.net

import android.net.Uri

/**
 * Returns the host without common prefixes like "www" or "m".
 */
@Suppress("MagicNumber")
val Uri.hostWithoutCommonPrefixes: String?
    get() {
        val host = host ?: return null

        return when {
            host.startsWith("www.") -> host.substring(4)
            host.startsWith("mobile.") -> host.substring(7)
            host.startsWith("m.") -> host.substring(2)
            else -> host
        }
    }

/**
 * Returns true if the [Uri] uses the "http" or "https" protocol scheme.
 */
val Uri.isHttpOrHttps: Boolean
    get() = (scheme?.equals("http") ?: false) || (scheme?.equals("https") ?: false)
