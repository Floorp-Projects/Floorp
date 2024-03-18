/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.publicsuffixlist

import android.content.Context
import kotlinx.coroutines.CoroutineDispatcher
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Deferred
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.async

/**
 * API for reading and accessing the public suffix list.
 *
 * > A "public suffix" is one under which Internet users can (or historically could) directly register names. Some
 * > examples of public suffixes are .com, .co.uk and pvt.k12.ma.us. The Public Suffix List is a list of all known
 * > public suffixes.
 *
 * Note that this implementation applies the rules of the public suffix list only and does not validate domains.
 *
 * https://publicsuffix.org/
 * https://github.com/publicsuffix/list
 */
class PublicSuffixList(
    context: Context,
    dispatcher: CoroutineDispatcher = Dispatchers.IO,
    private val scope: CoroutineScope = CoroutineScope(dispatcher),
) {
    private val data: PublicSuffixListData by lazy { PublicSuffixListLoader.load(context) }

    /**
     * Prefetch the public suffix list from disk so that it is available in memory.
     */
    fun prefetch(): Deferred<Unit> = scope.async {
        data.run { Unit }
    }

    /**
     * Returns true if the given [domain] is a public suffix; false otherwise.
     *
     * E.g.:
     * ```
     *   co.uk       -> true
     *   com         -> true
     *   mozilla.org -> false
     *   org         -> true
     * ```
     *
     * Note that this method ignores the default "prevailing rule" described in the formal public suffix list algorithm:
     * If no rule matches then the passed [domain] is assumed to *not* be a public suffix.
     *
     * @param [domain] _must_ be a valid domain. [PublicSuffixList] performs no validation, and if any unexpected values
     * are passed (e.g., a full URL, a domain with a trailing '/', etc) this may return an incorrect result.
     */
    fun isPublicSuffix(domain: String): Deferred<Boolean> = scope.async {
        when (data.getPublicSuffixOffset(domain)) {
            is PublicSuffixOffset.PublicSuffix -> true
            else -> false
        }
    }

    /**
     * Returns the public suffix and one more level; known as the registrable domain. Returns `null` if
     * [domain] is a public suffix itself.
     *
     * E.g.:
     * ```
     * wwww.mozilla.org -> mozilla.org
     * www.bcc.co.uk    -> bbc.co.uk
     * a.b.ide.kyoto.jp -> b.ide.kyoto.jp
     * ```
     *
     * @param [domain] _must_ be a valid domain. [PublicSuffixList] performs no validation, and if any unexpected values
     * are passed (e.g., a full URL, a domain with a trailing '/', etc) this may return an incorrect result.
     */
    fun getPublicSuffixPlusOne(domain: String): Deferred<String?> = scope.async {
        when (val offset = data.getPublicSuffixOffset(domain)) {
            is PublicSuffixOffset.Offset ->
                domain
                    .split('.')
                    .drop(offset.value)
                    .joinToString(separator = ".")
            else -> null
        }
    }

    /**
     * Returns the public suffix of the given [domain]; known as the effective top-level domain (eTLD). Returns `null`
     * if the [domain] is a public suffix itself.
     *
     * E.g.:
     * ```
     * wwww.mozilla.org -> org
     * www.bcc.co.uk    -> co.uk
     * a.b.ide.kyoto.jp -> ide.kyoto.jp
     * ```
     *
     * @param [domain] _must_ be a valid domain. [PublicSuffixList] performs no validation, and if any unexpected values
     * are passed (e.g., a full URL, a domain with a trailing '/', etc) this may return an incorrect result.
     */
    fun getPublicSuffix(domain: String) = scope.async {
        when (val offset = data.getPublicSuffixOffset(domain)) {
            is PublicSuffixOffset.Offset ->
                domain
                    .split('.')
                    .drop(offset.value + 1)
                    .joinToString(separator = ".")
            else -> null
        }
    }

    /**
     * Strips the public suffix from the given [domain]. Returns the original domain if no public suffix could be
     * stripped.
     *
     * E.g.:
     * ```
     * wwww.mozilla.org -> www.mozilla
     * www.bcc.co.uk    -> www.bbc
     * a.b.ide.kyoto.jp -> a.b
     * ```
     *
     * @param [domain] _must_ be a valid domain. [PublicSuffixList] performs no validation, and if any unexpected values
     * are passed (e.g., a full URL, a domain with a trailing '/', etc) this may return an incorrect result.
     */
    fun stripPublicSuffix(domain: String) = scope.async {
        when (val offset = data.getPublicSuffixOffset(domain)) {
            is PublicSuffixOffset.Offset ->
                domain
                    .split('.')
                    .joinToString(separator = ".", limit = offset.value + 1, truncated = "")
                    .dropLast(1)
            else -> domain
        }
    }
}
