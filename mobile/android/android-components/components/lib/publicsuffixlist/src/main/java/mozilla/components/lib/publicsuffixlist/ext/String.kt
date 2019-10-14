/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.publicsuffixlist.ext

import android.net.Uri
import kotlinx.coroutines.CompletableDeferred
import kotlinx.coroutines.Deferred
import mozilla.components.lib.publicsuffixlist.PublicSuffixList
import mozilla.components.support.ktx.android.net.hostWithoutCommonPrefixes
import java.net.MalformedURLException

/**
 * Trim a host's prefix and suffix
 */
fun String.urlToTrimmedHost(publicSuffixList: PublicSuffixList): Deferred<String> {
    return try {
        val host = Uri.parse(this).hostWithoutCommonPrefixes ?: return CompletableDeferred(this)
        publicSuffixList.stripPublicSuffix(host)
    } catch (e: MalformedURLException) {
        CompletableDeferred(this)
    }
}
