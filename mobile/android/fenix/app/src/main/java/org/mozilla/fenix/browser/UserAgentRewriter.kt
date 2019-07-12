/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.browser

import android.os.Build.VERSION.RELEASE
import mozilla.components.concept.engine.EngineSession
import mozilla.components.support.ktx.kotlin.sha1

/**
 * Utility to rewrite the User-Agent header for requests to whitelisted domains.
 *
 * Follow up: https://github.com/mozilla-mobile/fenix/issues/3341
 */
object UserAgentRewriter {

    /**
     * Updates the User-Agent based on the [whitelistUaFilter] to set the [userAgentGecko69] value.
     */
    fun maybeRewriteUserAgent(session: EngineSession, host: String) {
        session.settings.userAgentString = if (whitelistUaFilter.contains(host.sha1())) {
            userAgentGecko69
        } else {
            null
        }
    }

    /**
     * The white-listed domains.
     */
    private val whitelistUaFilter = setOf(
        "1b12ecd917215146f79a0ac5e01b3059faadab47",
        "a486f819018512f60a8a66324e51be0e1118a91d"
    )

    /**
     * The User-Agent to use for the white-listed domains.
     */
    private val userAgentGecko69 = "Mozilla/5.0 (Android $RELEASE; Mobile; rv:69.0) Gecko/69.0 Firefox/69.0"
}
