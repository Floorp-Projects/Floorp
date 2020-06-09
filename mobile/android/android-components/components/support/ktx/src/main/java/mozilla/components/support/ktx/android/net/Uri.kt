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

/**
 * Checks that the given URL is in scope of the web app specified by [trustedScopes].
 *
 * https://www.w3.org/TR/appmanifest/#dfn-within-scope
 */
fun Uri.isInScope(trustedScopes: Iterable<Uri>): Boolean {
    val path = path.orEmpty()
    return trustedScopes.any { scope ->
        sameOriginAs(scope) && path.startsWith(scope.path.orEmpty())
    }
}

/**
 * Checks that Uri has the same scheme and host as [other].
 */
fun Uri.sameSchemeAndHostAs(other: Uri) = scheme == other.scheme && host == other.host

/**
 * Checks that Uri has the same origin as [other].
 *
 * https://html.spec.whatwg.org/multipage/origin.html#same-origin
 */
fun Uri.sameOriginAs(other: Uri) = sameSchemeAndHostAs(other) && port == other.port
