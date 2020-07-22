/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.logins

import mozilla.components.lib.publicsuffixlist.PublicSuffixList
import org.mozilla.fenix.ext.urlToTrimmedHost

sealed class SortingStrategy {
    abstract operator fun invoke(logins: List<SavedLogin>): List<SavedLogin>

    data class Alphabetically(private val publicSuffixList: PublicSuffixList) : SortingStrategy() {
        override fun invoke(logins: List<SavedLogin>): List<SavedLogin> {
            return logins.sortedBy { it.origin.urlToTrimmedHost(publicSuffixList) }
        }
    }

    object LastUsed : SortingStrategy() {
        override fun invoke(logins: List<SavedLogin>): List<SavedLogin> {
            return logins.sortedByDescending { it.timeLastUsed }
        }
    }
}
