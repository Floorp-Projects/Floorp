/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.pwa.ext

import android.net.Uri

/**
 * Returns just the origin of the [Uri].
 *
 * The origin is a URL that contains only the scheme, host, and port.
 * https://html.spec.whatwg.org/multipage/origin.html#concept-origin
 *
 * Null is returned if the URI was invalid (i.e.: `"/foo/bar".toUri()`)
 */
fun Uri.toOrigin(): Uri? {
    var authority = host
    if (port != -1) {
        authority += ":$port"
    }

    val result = Uri.Builder().scheme(scheme).encodedAuthority(authority).build().normalizeScheme()
    return if (result.toString().isNotEmpty()) result else null
}
