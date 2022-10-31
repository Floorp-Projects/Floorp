/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:SuppressWarnings("MaxLineLength")

package mozilla.components.support.utils

import android.webkit.URLUtil
import java.net.URI
import java.net.URISyntaxException
import java.util.LinkedList
import java.util.Locale
import java.util.regex.Pattern

/**
 * Regular expressions used in this class are taken from Android's Patterns.java.
 * We brought them in to standardize URL matching across Android versions, instead of relying
 * on Android version-dependent built-ins that can vary across Android versions.
 * The original code can be found here:
 * http://androidxref.com/8.0.0_r4/xref/frameworks/base/core/java/android/util/Patterns.java
 */
class WebURLFinder {
    private val candidates: List<String>

    constructor(string: String?) {
        if (string == null) {
            throw IllegalArgumentException("string must not be null")
        }
        this.candidates = candidateWebURLs(string)
    }

    /* package-private */ internal constructor(string: String?, explicitUnicode: Boolean) {
        if (string == null) {
            throw IllegalArgumentException("strings must not be null")
        }
        this.candidates = candidateWebURLs(string, explicitUnicode)
    }

    /* package-private */ internal constructor(strings: List<String>?, explicitUnicode: Boolean) {
        if (strings == null) {
            throw IllegalArgumentException("strings must not be null")
        }
        this.candidates = candidateWebURLs(strings, explicitUnicode)
    }

    /**
     * Return best Web URL.
     *
     *
     * "Best" means a Web URL with a scheme, and failing that, a Web URL without a
     * scheme.
     *
     * @return a Web URL or `null`.
     */
    fun bestWebURL(): String? = firstWebURLWithScheme() ?: firstWebURLWithoutScheme()

    private fun firstWebURLWithScheme(): String? {
        candidates.forEach { match ->
            try {
                if (URI(match).scheme != null) {
                    return match
                }
            } catch (e: URISyntaxException) {
                // Ignore: on to the next.
            }
        }

        return null
    }

    private fun firstWebURLWithoutScheme(): String? = candidates.firstOrNull()

    @SuppressWarnings("LargeClass")
    companion object {
        // Taken from mozilla.components.support.utils.URLStringUtils. See documentation
        // there for a complete description.
        private const val autolinkWebUrlPattern = "(\\w+-)*\\w+(://[/]*|:|\\.)(\\w+-)*\\w+([\\S&&[^\\w-]]\\S*)?"

        private val autolinkWebUrl by lazy {
            Pattern.compile(autolinkWebUrlPattern, 0)
        }

        private val autolinkWebUrlExplicitUnicode by lazy {
            // To run tests on a non-Android device (like a computer), Pattern.compile
            // requires a flag to enable unicode support. Set a value like flags here with a local
            // copy of UNICODE_CHARACTER_CLASS. Use a local copy because that constant is not
            // available on Android platforms < 24 (Fenix targets 21). At runtime this is not an issue
            // because, again, Android REs are always unicode compliant.
            // NB: The value has to go through an intermediate variable; otherwise, the linter will
            // complain that this value is not one of the predefined enums that are allowed.
            @Suppress("MagicNumber")
            val UNICODE_CHARACTER_CLASS: Int = 0x100
            var regexFlags = UNICODE_CHARACTER_CLASS
            Pattern.compile(autolinkWebUrlPattern, regexFlags)
        }

        /**
         * Check if string is a Web URL.
         *
         *
         * A Web URL is a URI that is not a `file:` or
         * `javascript:` scheme.
         *
         * @param string to check.
         * @return `true` if `string` is a Web URL.
         */
        @SuppressWarnings("TooGenericExceptionCaught")
        fun isWebURL(string: String): Boolean {
            try {
                URI(string)
            } catch (e: Exception) {
                return false
            }

            return !(URLUtil.isFileUrl(string.lowercase(Locale.ROOT)) || URLUtil.isJavaScriptUrl(string.lowercase(Locale.ROOT)))
        }

        private fun candidateWebURLs(strings: Collection<String?>, explicitUnicode: Boolean = false): List<String> {
            val candidates = mutableListOf<String>()

            // no functional transformation lambdas (ie. flatMapNotNull) since it would turn an
            // O(n) loop into an O(2n) loop
            for (string in strings) {
                if (string == null) {
                    continue
                }

                candidates.addAll(candidateWebURLs(string, explicitUnicode))
            }

            return candidates
        }

        private fun candidateWebURLs(string: String, explicitUnicode: Boolean = false): List<String> {
            val matcher = when {
                explicitUnicode -> autolinkWebUrlExplicitUnicode.matcher(string)
                else -> autolinkWebUrl.matcher(string)
            }

            val matches = LinkedList<String>()

            while (matcher.find()) {
                // Remove URLs with bad schemes.
                if (!isWebURL(matcher.group())) {
                    continue
                }

                // Remove parts of email addresses.
                if (matcher.start() > 0 && string[matcher.start() - 1] == '@') {
                    continue
                }

                matches.add(matcher.group())
            }

            return matches
        }
    }
}
