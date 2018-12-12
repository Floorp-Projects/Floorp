/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
@file:Suppress("MatchingDeclarationName")

package mozilla.components.support.utils

import java.net.MalformedURLException
import java.net.URL

const val WWW_PREFIX_OFFSET = 4

data class DomainMatch(val url: String, val matchedSegment: String)

// FIXME implement Fennec-style segment matching logic
// See https://github.com/mozilla-mobile/android-components/issues/1279
fun segmentAwareDomainMatch(query: String, urls: Iterable<String>): DomainMatch? {
    return basicMatch(query, urls)?.let { matchedUrl ->
        matchSegment(query, matchedUrl)?.let { DomainMatch(matchedUrl, it) }
    }
}

@SuppressWarnings("ReturnCount")
private fun basicMatch(query: String, urls: Iterable<String>): String? {
    for (rawUrl in urls) {
        if (rawUrl.startsWith(query)) {
            return rawUrl
        }

        val host = (try { URL(rawUrl) } catch (e: MalformedURLException) { null })?.host ?: ""
        if (host.startsWith(query)) {
            return rawUrl
        }
        val strippedHost = if (host.startsWith("www.")) {
            host.substring(WWW_PREFIX_OFFSET)
        } else {
            host
        }
        if (strippedHost.startsWith(query)) {
            return rawUrl
        }
    }
    return null
}

private fun matchSegment(query: String, rawUrl: String): String? {
    if (rawUrl.startsWith(query)) { return rawUrl }
    val url = URL(rawUrl)
    if (url.host.startsWith(query)) { return url.host + url.path }
    // Strip "www".
    return url.host.substring(WWW_PREFIX_OFFSET) + url.path
}
