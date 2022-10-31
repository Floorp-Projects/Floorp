/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.utils

import android.content.Context

/**
 * Collection of methods used for ensuring the validity and security of URLs.
 */
object SafeUrl {
    /**
     * Remove recursively the schemes declared in [R.array.mozac_url_schemes_blocklist]
     * from the front of [unsafeText].
     */
    fun stripUnsafeUrlSchemes(context: Context, unsafeText: CharSequence?): String? {
        val urlSchemesBlocklist = context.resources.getStringArray(R.array.mozac_url_schemes_blocklist)
        var safeUrl = unsafeText.toString()
        if (safeUrl.isEmpty()) {
            return safeUrl
        }

        @Suppress("ControlFlowWithEmptyBody", "EmptyWhileBlock")
        while (urlSchemesBlocklist.find {
            if (safeUrl.startsWith(it, true)) {
                safeUrl = safeUrl.replaceFirst(Regex(it, RegexOption.IGNORE_CASE), "")
                true
            } else {
                false
            }
        } != null
        ) {
        }

        return safeUrl
    }
}
