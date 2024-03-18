/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.publicsuffixlist

import mozilla.components.lib.publicsuffixlist.ext.binarySearch
import java.net.IDN

/**
 * Class wrapping the public suffix list data and offering methods for accessing rules in it.
 */
internal class PublicSuffixListData(
    private val rules: ByteArray,
    private val exceptions: ByteArray,
) {
    private fun binarySearchRules(labels: List<ByteArray>, labelIndex: Int): String? {
        return rules.binarySearch(labels, labelIndex)
    }

    private fun binarySearchExceptions(labels: List<ByteArray>, labelIndex: Int): String? {
        return exceptions.binarySearch(labels, labelIndex)
    }

    @Suppress("ReturnCount")
    fun getPublicSuffixOffset(domain: String): PublicSuffixOffset? {
        if (domain.isEmpty()) {
            return null
        }

        val domainLabels = IDN.toUnicode(domain).split('.')
        if (domainLabels.find { it.isEmpty() } != null) {
            // At least one of the labels is empty: Bail out.
            return null
        }

        val rule = findMatchingRule(domainLabels)

        if (domainLabels.size == rule.size && rule[0][0] != PublicSuffixListData.EXCEPTION_MARKER) {
            // The domain is a public suffix.
            return if (rule == PublicSuffixListData.PREVAILING_RULE) {
                PublicSuffixOffset.PrevailingRule
            } else {
                PublicSuffixOffset.PublicSuffix
            }
        }

        return if (rule[0][0] == PublicSuffixListData.EXCEPTION_MARKER) {
            // Exception rules hold the effective TLD plus one.
            PublicSuffixOffset.Offset(domainLabels.size - rule.size)
        } else {
            // Otherwise the rule is for a public suffix, so we must take one more label.
            PublicSuffixOffset.Offset(domainLabels.size - (rule.size + 1))
        }
    }

    /**
     * Find a matching rule for the given domain labels.
     *
     * This algorithm is based on OkHttp's PublicSuffixDatabase class:
     * https://github.com/square/okhttp/blob/master/okhttp/src/main/java/okhttp3/internal/publicsuffix/PublicSuffixDatabase.java
     */
    private fun findMatchingRule(domainLabels: List<String>): List<String> {
        // Break apart the domain into UTF-8 labels, i.e. foo.bar.com turns into [foo, bar, com].
        val domainLabelsBytes = domainLabels.map { it.toByteArray(Charsets.UTF_8) }

        val exactMatch = findExactMatch(domainLabelsBytes)
        val wildcardMatch = findWildcardMatch(domainLabelsBytes)
        val exceptionMatch = findExceptionMatch(domainLabelsBytes, wildcardMatch)

        if (exceptionMatch != null) {
            return ("${PublicSuffixListData.EXCEPTION_MARKER}$exceptionMatch").split('.')
        }

        if (exactMatch == null && wildcardMatch == null) {
            return PublicSuffixListData.PREVAILING_RULE
        }

        val exactRuleLabels = exactMatch?.split('.') ?: PublicSuffixListData.EMPTY_RULE
        val wildcardRuleLabels = wildcardMatch?.split('.') ?: PublicSuffixListData.EMPTY_RULE

        return if (exactRuleLabels.size > wildcardRuleLabels.size) {
            exactRuleLabels
        } else {
            wildcardRuleLabels
        }
    }

    /**
     * Returns an exact match or null.
     */
    private fun findExactMatch(labels: List<ByteArray>): String? {
        // Start by looking for exact matches. We start at the leftmost label. For example, foo.bar.com
        // will look like: [foo, bar, com], [bar, com], [com]. The longest matching rule wins.

        for (i in 0 until labels.size) {
            val rule = binarySearchRules(labels, i)

            if (rule != null) {
                return rule
            }
        }

        return null
    }

    /**
     * Returns a wildcard match or null.
     */
    private fun findWildcardMatch(labels: List<ByteArray>): String? {
        // In theory, wildcard rules are not restricted to having the wildcard in the leftmost position.
        // In practice, wildcards are always in the leftmost position. For now, this implementation
        // cheats and does not attempt every possible permutation. Instead, it only considers wildcards
        // in the leftmost position. We assert this fact when we generate the public suffix file. If
        // this assertion ever fails we'll need to refactor this implementation.
        if (labels.size > 1) {
            val labelsWithWildcard = labels.toMutableList()
            for (labelIndex in 0 until labelsWithWildcard.size) {
                labelsWithWildcard[labelIndex] = PublicSuffixListData.WILDCARD_LABEL
                val rule = binarySearchRules(labelsWithWildcard, labelIndex)
                if (rule != null) {
                    return rule
                }
            }
        }

        return null
    }

    private fun findExceptionMatch(labels: List<ByteArray>, wildcardMatch: String?): String? {
        // Exception rules only apply to wildcard rules, so only try it if we matched a wildcard.
        if (wildcardMatch == null) {
            return null
        }

        for (labelIndex in 0 until labels.size) {
            val rule = binarySearchExceptions(labels, labelIndex)
            if (rule != null) {
                return rule
            }
        }

        return null
    }

    companion object {
        val WILDCARD_LABEL = byteArrayOf('*'.code.toByte())
        val PREVAILING_RULE = listOf("*")
        val EMPTY_RULE = listOf<String>()
        const val EXCEPTION_MARKER = '!'
    }
}

internal sealed class PublicSuffixOffset {
    data class Offset(val value: Int) : PublicSuffixOffset()
    object PublicSuffix : PublicSuffixOffset()
    object PrevailingRule : PublicSuffixOffset()
}
