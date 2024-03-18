/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:Suppress("MatchingDeclarationName")

package mozilla.components.support.utils

import android.net.Uri
import java.net.MalformedURLException
import java.util.Locale

data class DomainMatch(val url: String, val matchedSegment: String)

// FIXME implement Fennec-style segment matching logic
// See https://github.com/mozilla-mobile/android-components/issues/1279
fun segmentAwareDomainMatch(query: String, urls: Iterable<String>): DomainMatch? {
    val locale = Locale.US

    val caseInsensitiveQuery = query.lowercase(locale)
    // Process input 'urls' lazily, as the list could be very large and likely we'll find a match
    // by going through just a small subset.
    val caseInsensitiveUrls = urls.asSequence().map { it.lowercase(locale) }

    return basicMatch(caseInsensitiveQuery, caseInsensitiveUrls)?.let { matchedUrl ->
        matchSegment(caseInsensitiveQuery, matchedUrl)?.let { DomainMatch(matchedUrl, it) }
    }
}

/**
 * Check if [url] starts with the exact [text] or if [url]'s domain start with the exact [text].
 *
 * @return `true` if [url] starts with [text], `false` otherwise.
 */
fun doesUrlStartsWithText(url: String, text: String) = findUrlMatchingText(url, text) != null

private fun basicMatch(query: String, urls: Sequence<String>): String? {
    return urls.firstOrNull { findUrlMatchingText(it, query) != null }
}

@SuppressWarnings("ReturnCount")
private fun findUrlMatchingText(url: String, text: String): String? {
    if (url.startsWith(text)) {
        return url
    }

    val uri = try {
        Uri.parse(url)
    } catch (e: MalformedURLException) {
        null
    }

    var urlSansProtocol = uri?.host
    urlSansProtocol += uri?.port?.orEmpty() + uri?.path
    urlSansProtocol?.let {
        if (it.startsWith(text) || it.noCommonSubdomains().startsWith(text)) {
            return url
        }
    }

    val host = uri?.host ?: ""

    if (host.startsWith(text)) {
        return url
    }

    if (host.noCommonSubdomains().startsWith(text)) {
        return url
    }

    return null
}

private fun matchSegment(query: String, rawUrl: String): String? {
    if (rawUrl.startsWith(query)) {
        return rawUrl
    }

    val url = Uri.parse(rawUrl)
    return url.host?.let { host ->
        if (host.startsWith(query)) {
            host + url.port.orEmpty() + url.path
        } else {
            val strippedHost = host.noCommonSubdomains()
            if (strippedHost != url.host) {
                strippedHost + url.port.orEmpty() + url.path
            } else {
                host + url.port.orEmpty() + url.path
            }
        }
    }
}

private fun String.noCommonSubdomains(): String {
    // This kind of stripping allows us to match "twitter" to "mobile.twitter.com".
    val domainsToStrip = listOf("www", "mobile", "m")

    domainsToStrip.forEach { domain ->
        if (this.startsWith(domain)) {
            return this.substring(domain.length + 1)
        }
    }
    return this
}

private fun Int?.orEmpty(): String {
    return this.takeIf { it != -1 }?.let { ":$it" }.orEmpty()
}
