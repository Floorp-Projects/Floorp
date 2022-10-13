/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.utils

import android.net.Uri
import android.text.TextUtils
import androidx.annotation.VisibleForTesting
import androidx.core.text.TextDirectionHeuristicCompat
import androidx.core.text.TextDirectionHeuristicsCompat.FIRSTSTRONG_LTR
import java.util.regex.Pattern

object URLStringUtils {
    /**
     * Determine whether a string is a URL.
     *
     * This method performs a lenient check to determine whether a string is a URL. Anything that
     * contains a :, ://, or . and has no internal spaces is potentially a URL. If you need a
     * stricter check, consider using isURLLikeStrict().
     */
    fun isURLLike(string: String) = isURLLenient.matcher(string).matches()

    /**
     * Determine whether a string is a search term.
     *
     * This method recognizes a string as a search term as anything other than a URL.
     */
    fun isSearchTerm(string: String) = !isURLLike(string)

    /**
     * Normalizes a URL String.
     */
    fun toNormalizedURL(string: String): String {
        val trimmedInput = string.trim()
        var uri = Uri.parse(trimmedInput)
        if (TextUtils.isEmpty(uri.scheme)) {
            uri = Uri.parse("http://$trimmedInput")
        } else {
            uri = uri.normalizeScheme()
        }
        return uri.toString()
    }

    private val isURLLenient by lazy {
        // Be lenient about what is classified as potentially a URL.
        // (\w+-+)*\w+(://[/]*|:|\.)(\w+-+)*\w+([\S&&[^\w-]]\S*)?
        // -------                 -------
        // 0 or more pairs of consecutive word letters or dashes
        //        ---                     ---
        // followed by at least a single word letter.
        // -----------             ----------
        // Combined, that means "w", "w-w", "w-w-w", etc match, but "w-", "w-w-", "w-w-w-" do not.
        //          --------------
        // That surrounds :, :// or .
        //                                                    -
        // At the end, there may be an optional
        //                                    ------------
        // non-word, non-- but still non-space character (e.g., ':', '/', '.', '?' but not 'a', '-', '\t')
        //                                                ---
        // and 0 or more non-space characters.
        //
        // These are some (odd) examples of valid urls according to this pattern:
        // c-c.com
        // c-c-c-c.c-c-c
        // c-http://c.com
        // about-mozilla:mozilla
        // c-http.d-x
        // www.c-
        // 3-3.3
        // www.c-c.-
        //
        // There are some examples of non-URLs according to this pattern:
        // -://x.com
        // -x.com
        // http://www-.com
        // www.c-c-
        // 3-3
        Pattern.compile("^\\s*(\\w+-+)*\\w+(://[/]*|:|\\.)(\\w+-+)*\\w+([\\S&&[^\\w-]]\\S*)?\\s*$", flags)
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal const val UNICODE_CHARACTER_CLASS: Int = 0x100

    // To run tests on a non-Android device (like a computer), Pattern.compile
    // requires a flag to enable unicode support. Set a value like flags here with a local
    // copy of UNICODE_CHARACTER_CLASS. Use a local copy because that constant is not
    // available on Android platforms < 24 (Fenix targets 21). At runtime this is not an issue
    // because, again, Android REs are always unicode compliant.
    // NB: The value has to go through an intermediate variable; otherwise, the linter will
    // complain that this value is not one of the predefined enums that are allowed.
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal var flags = 0

    private const val HTTP = "http://"
    private const val HTTPS = "https://"
    private const val WWW = "www."

    /**
     * Generates a shorter version of the provided URL for display purposes by stripping it of
     * https/http and/or WWW prefixes and/or trailing slash when applicable.
     *
     * The returned text will always be displayed from left to right.
     * If the directionality would otherwise be RTL "\u200E" will be prepended to the result to force LTR.
     */
    fun toDisplayUrl(
        originalUrl: CharSequence,
        textDirectionHeuristic: TextDirectionHeuristicCompat = FIRSTSTRONG_LTR,
    ): CharSequence {
        val strippedText = maybeStripTrailingSlash(maybeStripUrlProtocol(originalUrl))

        return if (
            strippedText.isNotBlank() &&
            textDirectionHeuristic.isRtl(strippedText, 0, 1)
        ) {
            "\u200E" + strippedText
        } else {
            strippedText
        }
    }

    private fun maybeStripUrlProtocol(url: CharSequence): CharSequence {
        var noPrefixUrl = url
        if (url.toString().startsWith(HTTPS)) {
            noPrefixUrl = maybeStripUrlSubDomain(url.toString().replaceFirst(HTTPS, ""))
        } else if (url.toString().startsWith(HTTP)) {
            noPrefixUrl = maybeStripUrlSubDomain(url.toString().replaceFirst(HTTP, ""))
        }
        return noPrefixUrl
    }

    private fun maybeStripUrlSubDomain(url: CharSequence): CharSequence {
        return if (url.toString().startsWith(WWW)) {
            url.toString().replaceFirst(WWW, "")
        } else {
            url
        }
    }

    private fun maybeStripTrailingSlash(url: CharSequence): CharSequence {
        return url.trimEnd('/')
    }
}
