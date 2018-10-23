/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.geckoview

import org.mozilla.gecko.util.GeckoBundle

/**
 * Creates a mocked WebResponseInfo object.
 *
 * We need to jump through several hoops here:
 *  1) We can't mock WebResponseInfo because values are accessed via properties and not methods.
 *  2) The constructor of WebResponseInfo has 'package' visibility so we need to create it from
 *     inside the org.mozilla.geckoview package.
 *  3) WebResponseInfo is not static, so it needs to be created from inside a GeckoSession.
 *
 *  https://bugzilla.mozilla.org/show_bug.cgi?id=1476552
 */
fun createMockedWebResponseInfo(
    uri: String,
    contentType: String,
    contentLength: Long,
    filename: String
): GeckoSession.WebResponseInfo {
    val session = object : GeckoSession(null) {
        fun createMockedWebResponseInfo(): WebResponseInfo {
            val bundle = GeckoBundle()
            bundle.putString("uri", uri)
            bundle.putString("contentType", contentType)
            bundle.putLong("contentLength", contentLength)
            bundle.putString("filename", filename)

            return WebResponseInfo(bundle)
        }
    }

    return session.createMockedWebResponseInfo()
}
