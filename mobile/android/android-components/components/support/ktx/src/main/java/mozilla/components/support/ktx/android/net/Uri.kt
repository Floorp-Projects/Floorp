/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.ktx.android.net

import android.content.ContentResolver
import android.content.Context
import android.net.Uri
import android.os.Build
import java.io.File
import java.io.IOException

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
 * Checks that the given URL is in one of the given URL [scopes].
 *
 * https://www.w3.org/TR/appmanifest/#dfn-within-scope
 *
 * @param scopes Uris that each represent a scope.
 * A Uri is within the scope if the origin matches and it starts with the scope's path.
 * @return True if this Uri is within any of the given scopes.
 */
fun Uri.isInScope(scopes: Iterable<Uri>): Boolean {
    val path = path.orEmpty()
    return scopes.any { scope ->
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

/**
 * Indicate if the [this] uri is under the application private directory.
 */
fun Uri.isUnderPrivateAppDirectory(context: Context): Boolean {
    return when (this.scheme) {
        ContentResolver.SCHEME_FILE -> {
            try {
                val uriPath = path ?: return true
                val uriCanonicalPath = File(uriPath).canonicalPath
                val dataDirCanonicalPath = File(context.applicationInfo.dataDir).canonicalPath
                if (Build.VERSION.SDK_INT <= Build.VERSION_CODES.Q) {
                    uriCanonicalPath.startsWith(dataDirCanonicalPath)
                } else {
                    // We have to do this manual check on early builds of Android 11
                    // as symlink didn't resolve from /data/user/ to data/data
                    // we have to revise this again once Android 11 is out
                    // https://github.com/mozilla-mobile/android-components/issues/7750
                    uriCanonicalPath.startsWith("/data/data") || uriCanonicalPath.startsWith("/data/user")
                }
            } catch (e: IOException) {
                true
            }
        }
        else -> false
    }
}
