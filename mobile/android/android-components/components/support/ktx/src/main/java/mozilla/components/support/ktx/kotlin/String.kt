/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.ktx.kotlin

import android.net.Uri
import mozilla.components.support.utils.URLStringUtils
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

/**
 * A collection of regular expressions used in the `is*` methods below.
 */
private val re = object {
    val phoneish = "^\\s*tel:\\S?\\d+\\S*\\s*$".toRegex(RegexOption.IGNORE_CASE)
    val emailish = "^\\s*mailto:\\w+\\S*\\s*$".toRegex(RegexOption.IGNORE_CASE)
    val geoish = "^\\s*geo:\\S*\\d+\\S*\\s*$".toRegex(RegexOption.IGNORE_CASE)
}

/**
 * Checks if this String is a URL.
 */
fun String.isUrl() = URLStringUtils.isURLLike(this)

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
    return if (!this.isEmpty()) {
        formatter.parse(this)
    } else {
        Date()
    }
}

/**
 * Converts a [String] to a [Uri] object.
 */
fun String.toUri() = Uri.parse(this)
