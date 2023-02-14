/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ext

import androidx.annotation.VisibleForTesting
import org.mozilla.fenix.home.recenttabs.RecentTab

/**
 * Removes a [RecentTab.Tab] from a list of [RecentTab].
 *
 * @param tab [RecentTab] to remove from the list
 */
@VisibleForTesting
internal fun List<RecentTab>.filterOutTab(tab: RecentTab): List<RecentTab> = filterNot {
    it is RecentTab.Tab && tab is RecentTab.Tab && it.state.id == tab.state.id
}
