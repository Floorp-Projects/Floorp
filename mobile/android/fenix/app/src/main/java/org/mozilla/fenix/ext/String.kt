/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ext

import android.content.Context
import androidx.core.net.toUri
import kotlinx.coroutines.runBlocking
import java.net.MalformedURLException
import java.net.URL
import mozilla.components.support.ktx.android.net.hostWithoutCommonPrefixes

/**
 * Replaces the keys with the values with the map provided.
 */
fun String.replace(pairs: Map<String, String>): String {
    var result = this
    pairs.forEach { (l, r) -> result = result.replace(l, r) }
    return result
}

/**
 * Try to parse and get host part if this [String] is valid URL.
 * Returns **null** otherwise.
 */
fun String?.getHostFromUrl(): String? = try {
    URL(this).host
} catch (e: MalformedURLException) {
    null
}

/**
 * Trim a host's prefix and suffix
 */
fun String.urlToTrimmedHost(context: Context): String {
    return try {
        val host = toUri().hostWithoutCommonPrefixes ?: return this
        runBlocking {
            context.components.publicSuffixList.stripPublicSuffix(host).await()
        }
    } catch (e: MalformedURLException) {
        this
    }
}
