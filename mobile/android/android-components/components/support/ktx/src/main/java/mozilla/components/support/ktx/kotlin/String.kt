/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:Suppress("TooManyFunctions")

package mozilla.components.support.ktx.kotlin

import android.net.InetAddresses
import android.net.Uri
import android.os.Build
import android.util.Patterns
import android.webkit.URLUtil
import androidx.core.net.toUri
import mozilla.components.lib.publicsuffixlist.PublicSuffixList
import mozilla.components.support.ktx.android.net.commonPrefixes
import mozilla.components.support.ktx.android.net.hostWithoutCommonPrefixes
import mozilla.components.support.ktx.util.URLStringUtils
import java.io.File
import java.net.IDN
import java.net.MalformedURLException
import java.net.URL
import java.net.URLEncoder
import java.security.MessageDigest
import java.text.ParseException
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale
import kotlin.text.RegexOption.IGNORE_CASE

/**
 * A collection of regular expressions used in the `is*` methods below.
 */
private val re = object {
    val phoneish = "^\\s*tel:\\S?\\d+\\S*\\s*$".toRegex(IGNORE_CASE)
    val emailish = "^\\s*mailto:\\w+\\S*\\s*$".toRegex(IGNORE_CASE)
    val geoish = "^\\s*geo:\\S*\\d+\\S*\\s*$".toRegex(IGNORE_CASE)
}

private const val MAILTO = "mailto:"

// Number of last digits to be shown when credit card number is obfuscated.
private const val LAST_VISIBLE_DIGITS_COUNT = 4

// This is used for truncating URLs to prevent extreme cases from
// slowing down UI rendering e.g. in case of a bookmarklet or a data URI.
// https://github.com/mozilla-mobile/android-components/issues/5249
const val MAX_URI_LENGTH = 25000

private const val FILE_PREFIX = "file://"
private const val MAX_VALID_PORT = 65_535

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
// Unused Parameter: We may resume stripping eTLD, depending on conversations between security and UX
// Return count: This is a complex method, but it would not be more understandable if broken up
// ComplexCondition: Breaking out the complex condition would make this logic harder to follow
@Suppress("UNUSED_PARAMETER", "ReturnCount", "ComplexCondition")
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

    if (uri.host?.isIpv4OrIpv6() == true ||
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
@Suppress("DEPRECATION")
internal fun String.isIpv4(): Boolean = Patterns.IP_ADDRESS.matcher(this).matches()

// impl via FFiOS: https://github.com/mozilla-mobile/firefox-ios/blob/deb9736c905cdf06822ecc4a20152df7b342925d/Shared/Extensions/NSURLExtensions.swift#L292
// True IPv6 validation is difficult. This is slightly better than nothing
internal fun String.isIpv6(): Boolean {
    return this.isNotEmpty() && this.contains(":")
}

/**
 * Returns true if the string represents a valid Ipv4 or Ipv6 IP address.
 * Note: does not validate a dual format Ipv6 ( "y:y:y:y:y:y:x.x.x.x" format).
 *
 */
@Suppress("TooManyFunctions")
fun String.isIpv4OrIpv6(): Boolean {
    return if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
        InetAddresses.isNumericAddress(this)
    } else {
        this.isIpv4() || this.isIpv6()
    }
}

/**
 * Checks if this String is a URL.
 */
fun String.isUrl() = URLStringUtils.isURLLike(this)

/**
 * Checks if this String is a URL of an extension page.
 */
fun String.isExtensionUrl() = this.startsWith("moz-extension://")

/**
 * Checks if this String is a URL of a resource.
 */
fun String.isResourceUrl() = this.startsWith("resource://")

/**
 * Appends `http` scheme if no scheme is present in this String.
 */
fun String.toNormalizedUrl(): String {
    val s = this.trim()
    // Most commonly we'll encounter http or https schemes.
    // For these, avoid running through toNormalizedURL as an optimization.
    return if (!s.startsWith("http://") &&
        !s.startsWith("https://")
    ) {
        URLStringUtils.toNormalizedURL(s)
    } else {
        s
    }
}

fun String.isPhone() = re.phoneish.matches(this)

fun String.isEmail() = re.emailish.matches(this)

fun String.isGeoLocation() = re.geoish.matches(this)

/**
 * Converts a [String] to a [Date] object.
 * @param format date format used for formatting the this given [String] object.
 * @param locale the locale to use when converting the String, defaults to [Locale.ROOT].
 * @return a [Date] object with the values in the provided in this string, if empty string was provided, a current date
 * will be returned.
 */
fun String.toDate(format: String, locale: Locale = Locale.ROOT): Date {
    val formatter = SimpleDateFormat(format, locale)
    return if (isNotEmpty()) {
        formatter.parse(this) ?: Date()
    } else {
        Date()
    }
}

/**
 * Calculates a SHA1 hash for this string.
 */
@Suppress("MagicNumber")
fun String.sha1(): String {
    val characters = "0123456789abcdef"
    val digest = MessageDigest.getInstance("SHA-1").digest(toByteArray())
    return digest.joinToString(
        separator = "",
        transform = { byte ->
            String(charArrayOf(characters[byte.toInt() shr 4 and 0x0f], characters[byte.toInt() and 0x0f]))
        },
    )
}

/**
 * Tries to convert a [String] to a [Date] using a list of [possibleFormats].
 * @param possibleFormats one ore more possible format.
 * @return a [Date] object with the values in the provided in this string,
 * if the conversion is not possible null will be returned.
 */
fun String.toDate(
    vararg possibleFormats: String = arrayOf(
        "yyyy-MM-dd'T'HH:mm",
        "yyyy-MM-dd",
        "yyyy-'W'ww",
        "yyyy-MM",
        "HH:mm",
    ),
): Date? {
    possibleFormats.forEach {
        try {
            return this.toDate(it)
        } catch (pe: ParseException) {
            // move to next possible format
        }
    }
    return null
}

/**
 * Tries to parse and get host part if this [String] is valid URL.
 * Otherwise returns the string.
 */
fun String.tryGetHostFromUrl(): String = try {
    URL(this).host
} catch (e: MalformedURLException) {
    this
}

/**
 * Compares 2 URLs and returns true if they have the same origin,
 * which means: same protocol, same host, same port.
 * It will return false if either this or [other] is not a valid URL.
 */
fun String.isSameOriginAs(other: String): Boolean {
    fun canonicalizeOrigin(urlStr: String): String {
        val url = URL(urlStr)
        val port = if (url.port == -1) url.defaultPort else url.port
        val canonicalized = URL(url.protocol, url.host, port, "")
        return canonicalized.toString()
    }
    return try {
        canonicalizeOrigin(this) == canonicalizeOrigin(other)
    } catch (e: MalformedURLException) {
        false
    }
}

/**
 * Returns an origin (protocol, host and port) from an URL string.
 */
fun String.getOrigin(): String? {
    return try {
        val url = URL(this)
        val port = if (url.port == -1) url.defaultPort else url.port
        URL(url.protocol, url.host, port, "").toString()
    } catch (e: MalformedURLException) {
        null
    }
}

/**
 * Returns an origin without the default port.
 * For example for an input of "https://mozilla.org:443" you will get "https://mozilla.org".
 */
fun String.stripDefaultPort(): String {
    return try {
        val url = URL(this)
        val port = if (url.port == url.defaultPort) -1 else url.port
        URL(url.protocol, url.host, port, "").toString()
    } catch (e: MalformedURLException) {
        this
    }
}

/**
 * Remove any unwanted character in url like spaces at the beginning or end.
 */
fun String.sanitizeURL(): String {
    return this.trim()
}

/**
 * Remove any unwanted character from string containing file name.
 * For example for an input of "/../../../../../../directory/file.txt" you will get "file.txt"
 */
fun String.sanitizeFileName(): String {
    val file = File(this.substringAfterLast(File.separatorChar))
    // Remove unwanted subsequent dots in the file name.
    return if (file.extension.trim().isNotEmpty() && file.nameWithoutExtension.isNotEmpty()) {
        file.name.replace("\\.\\.+".toRegex(), ".")
    } else {
        file.name.replace(".", "")
    }
}

/**
 * Remove leading mailto from the string.
 * For example for an input of "mailto:example@example.com" you will get "example@example.com"
 */
fun String.stripMailToProtocol(): String {
    return if (this.startsWith(MAILTO)) {
        this.replaceFirst(MAILTO, "")
    } else {
        this
    }
}

/**
 * Translates the string into {@code application/x-www-form-urlencoded} string.
 */
fun String.urlEncode(): String {
    return URLEncoder.encode(this, Charsets.UTF_8.name())
}

/**
 * Returns the string if it's length is not higher than @param[maximumLength] or
 * a @param[replacement] string if String length is higher than @param[maximumLength]
 */
fun String.takeOrReplace(maximumLength: Int, replacement: String): String {
    return if (this.length > maximumLength) replacement else this
}

/**
 * Returns the extension (without ".") declared in the mime type of this data url.
 * In the event that this data url does not contain a mime type or image extension could be read
 * for any reason [defaultExtension] will be returned
 *
 * @param defaultExtension default extension if one could not be read from the mime type. Default is "jpg".
 */
fun String.getDataUrlImageExtension(defaultExtension: String = "jpg"): String {
    return ("data:image\\/([a-zA-Z0-9-.+]+).*").toRegex()
        .find(this)?.groups?.get(1)?.value ?: defaultExtension
}

/**
 * Returns this char sequence if it's not null or empty
 * or the result of calling [defaultValue] function if the char sequence is null or empty.
 */
inline fun <C, R> C?.ifNullOrEmpty(defaultValue: () -> R): C where C : CharSequence, R : C =
    if (isNullOrEmpty()) defaultValue() else this

/**
 * Get the representative part of the URL. Usually this is the eTLD part of the host.
 *
 * For example this method will return "facebook.com" for "https://www.facebook.com/foobar".
 */
fun String.getRepresentativeSnippet(): String {
    val uri = Uri.parse(this)

    val host = uri.hostWithoutCommonPrefixes
    if (!host.isNullOrEmpty()) {
        return host
    }

    val path = uri.path
    if (!path.isNullOrEmpty()) {
        return path
    }

    return this
}

/**
 * Get a representative character for the given URL.
 *
 * For example this method will return "f" for "https://m.facebook.com/foobar".
 */
fun String.getRepresentativeCharacter(): String {
    val snippet = this.getRepresentativeSnippet()

    snippet.forEach { character ->
        if (character.isLetterOrDigit()) {
            return character.uppercase()
        }
    }

    return "?"
}

/**
 * Strips common mobile subdomains from a [String].
 */
fun String.stripCommonSubdomains(): String {
    for (prefix in commonPrefixes) {
        if (this.startsWith(prefix)) return this.substring(prefix.length)
    }
    return this
}

/**
 * Returns the last 4 digits from a formatted credit card number string.
 */
fun String.last4Digits(): String {
    return this.takeLast(LAST_VISIBLE_DIGITS_COUNT)
}

/**
 * Returns a trimmed string. This is used to prevent extreme cases
 * from slowing down UI rendering with large strings.
 */
fun String.trimmed(): String {
    return this.take(MAX_URI_LENGTH)
}
