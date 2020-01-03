/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.utils

import android.net.Uri
import android.text.TextUtils
import androidx.annotation.VisibleForTesting
import java.util.regex.Pattern

object URLStringUtils {
    /**
     * Determine whether a string is a URL.
     *
     * This method performs a strict check to determine whether a string is a URL. It takes longer
     * to execute than isURLLike() but checks whether, e.g., the TLD is ICANN-recognized. Consider
     * using isURLLike() unless these guarantees are required.
     */
    fun isURLLikeStrict(string: String, safe: Boolean = false) =
        if (safe) {
            string.matches(WebURLFinder.fuzzyUrlRegex)
        } else {
            string.matches(WebURLFinder.fuzzyUrlNonWebRegex)
        }

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
        }
        return uri.toString()
    }

    private val isURLLenient by lazy {
        // Be lenient about what is classified as potentially a URL. Anything that contains a :,
        // ://, or . and has no internal spaces is potentially a URL.
        //
        // Use java.util.regex because it is always unicode aware on Android.
        // https://developer.android.com/reference/java/util/regex/Pattern.html

        // Use both the \w+ and \S* after the punctuation because they seem to match slightly
        // different things. The \S matches any non-whitespace character (e.g., '~') and \w
        // matches only word characters. In other words, the regex is requiring that there be a
        // non-symbol character somewhere after the ., : or :// and before any other character
        // or the end of the string. For example, match
        // mozilla.com/~userdir
        // and not
        // mozilla./~ or mozilla:/
        // Without the [/]* after the :// in the alternation of the characters required to be a
        // valid URL,
        // file:///home/user/myfile.html
        // is considered a search term; it is clearly a URL.
        Pattern.compile("^\\s*\\w+(://[/]*|:|\\.)\\w+\\S*\\s*$", flags)
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
}
