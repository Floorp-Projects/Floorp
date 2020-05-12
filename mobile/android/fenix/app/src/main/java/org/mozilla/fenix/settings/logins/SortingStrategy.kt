/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.logins

import android.content.Context
import org.mozilla.fenix.ext.urlToTrimmedHost

sealed class SortingStrategy {
    abstract operator fun invoke(logins: List<SavedLogin>): List<SavedLogin>
    abstract val appContext: Context

    data class Alphabetically(override val appContext: Context) : SortingStrategy() {
        override fun invoke(logins: List<SavedLogin>): List<SavedLogin> {
            return logins.sortedBy { it.origin.urlToTrimmedHost(appContext) }
        }
    }

    data class LastUsed(override val appContext: Context) : SortingStrategy() {
        override fun invoke(logins: List<SavedLogin>): List<SavedLogin> {
            return logins.sortedByDescending { it.timeLastUsed }
        }
    }
}
