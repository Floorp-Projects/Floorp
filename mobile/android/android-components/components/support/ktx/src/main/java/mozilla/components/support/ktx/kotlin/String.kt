/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:Suppress("TooManyFunctions")

package mozilla.components.support.ktx.kotlin

import mozilla.components.support.utils.URLStringUtils
import java.io.File
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

fun String.toNormalizedUrl() = URLStringUtils.toNormalizedURL(this)

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
    return digest.joinToString(separator = "", transform = { byte ->
        String(charArrayOf(characters[byte.toInt() shr 4 and 0x0f], characters[byte.toInt() and 0x0f]))
    })
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
            "HH:mm"
    )
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
    } else this
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
