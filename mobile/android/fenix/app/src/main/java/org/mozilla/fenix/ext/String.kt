/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ext

import android.graphics.Bitmap
import android.graphics.BitmapFactory
import android.net.Uri
import android.text.Editable
import android.util.Patterns
import android.webkit.URLUtil
import androidx.core.net.toUri
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import mozilla.components.concept.fetch.Client
import mozilla.components.concept.fetch.Request
import mozilla.components.lib.publicsuffixlist.PublicSuffixList
import mozilla.components.lib.publicsuffixlist.ext.urlToTrimmedHost
import mozilla.components.support.ktx.android.net.hostWithoutCommonPrefixes
import org.mozilla.fenix.perf.runBlockingIncrement
import java.io.IOException
import java.net.IDN
import java.util.Locale

const val FILE_PREFIX = "file://"
const val MAX_VALID_PORT = 65_535

/**
 * Shortens URLs to be more user friendly.
 *
 * The algorithm used to generate these strings is a combination of FF desktop 'top sites',
 * feedback from the security team, and documentation regarding url elision.  See
 * StringTest.kt for details.
 *
 * This method is complex because URLs have a lot of edge cases. Be sure to thoroughly unit
 * test any changes you make to it.
 */
@Suppress("UNUSED_PARAMETER", "ReturnCount", "ComplexCondition")
// Unused Parameter: We may resume stripping eTLD, depending on conversations between security and UX
// Return count: This is a complex method, but it would not be more understandable if broken up
// ComplexCondition: Breaking out the complex condition would make this logic harder to follow
fun String.toShortUrl(publicSuffixList: PublicSuffixList): String {
    val inputString = this
    val uri = inputString.toUri()

    if (
        inputString.isEmpty() ||
        !URLUtil.isValidUrl(inputString) ||
        inputString.startsWith(FILE_PREFIX) ||
        uri.port !in -1..MAX_VALID_PORT
    ) {
        return inputString
    }

    if (uri.host?.isIpv4() == true ||
        uri.isIpv6() ||
        // If inputString is just a hostname and not a FQDN, use the entire hostname.
        uri.host?.contains(".") == false
    ) {
        return uri.host ?: inputString
    }

    fun String.stripUserInfo(): String {
        val userInfo = this.toUri().encodedUserInfo
        return if (userInfo != null) {
            val infoIndex = this.indexOf(userInfo)
            this.removeRange(infoIndex..infoIndex + userInfo.length)
        } else {
            this
        }
    }
    fun String.stripPrefixes(): String = this.toUri().hostWithoutCommonPrefixes ?: this
    fun String.toUnicode() = IDN.toUnicode(this)

    return inputString
        .stripUserInfo()
        .lowercase(Locale.getDefault())
        .stripPrefixes()
        .toUnicode()
}

// impl via FFTV https://searchfox.org/mozilla-mobile/source/firefox-echo-show/app/src/main/java/org/mozilla/focus/utils/FormattedDomain.java#129
fun String.isIpv4(): Boolean = Patterns.IP_ADDRESS.matcher(this).matches()

// impl via FFiOS: https://github.com/mozilla-mobile/firefox-ios/blob/deb9736c905cdf06822ecc4a20152df7b342925d/Shared/Extensions/NSURLExtensions.swift#L292
// True IPv6 validation is difficult. This is slightly better than nothing
private fun Uri.isIpv6(): Boolean {
    val host = this.host ?: return false
    return host.isNotEmpty() && host.contains(":")
}

/**
 * Trim a host's prefix and suffix
 */
fun String.urlToTrimmedHost(publicSuffixList: PublicSuffixList): String =
    runBlockingIncrement {
        urlToTrimmedHost(publicSuffixList).await()
    }

/**
 * Trims a URL string of its scheme and common prefixes.
 *
 * This is intended to act much like [PublicSuffixList.getPublicSuffixPlusOne()] but unlike
 * that method, leaves the path, anchor, etc intact.
 *
 */
fun String.simplifiedUrl(): String {
    val afterScheme = this.substringAfter("://")
    for (prefix in listOf("www.", "m.", "mobile.")) {
        if (afterScheme.startsWith(prefix)) {
            return afterScheme.substring(prefix.length)
        }
    }
    return afterScheme
}

/**
 * Returns an [Editable] for the provided string.
 */
fun String.toEditable(): Editable = Editable.Factory.getInstance().newEditable(this)

suspend fun bitmapForUrl(url: String, client: Client): Bitmap? = withContext(Dispatchers.IO) {
    // Code below will cache it in Gecko's cache, which ensures that as long as we've fetched it once,
    // we will be able to display this avatar as long as the cache isn't purged (e.g. via 'clear user data').
    val body = try {
        client.fetch(Request(url, useCaches = true)).body
    } catch (e: IOException) {
        return@withContext null
    }
    body.useStream { BitmapFactory.decodeStream(it) }
}
