/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ext

import mozilla.components.feature.top.sites.TopSite
import org.mozilla.fenix.settings.SupportUtils

/**
 * Returns the type name of the [TopSite].
 */
fun TopSite.name(): String = when (this) {
    is TopSite.Default -> "DEFAULT"
    is TopSite.Frecent -> "FRECENT"
    is TopSite.Pinned -> "PINNED"
    is TopSite.Provided -> "PROVIDED"
}

/**
 * Returns a sorted list of [TopSite] with the default Google top site always appearing
 * as the first item.
 */
fun List<TopSite>.sort(): List<TopSite> {
    val defaultGoogleTopSiteIndex = this.indexOfFirst {
        it is TopSite.Default && it.url == SupportUtils.GOOGLE_URL
    }

    return if (defaultGoogleTopSiteIndex == -1) {
        this
    } else {
        val result = this.toMutableList()
        result.removeAt(defaultGoogleTopSiteIndex)
        result.add(0, this[defaultGoogleTopSiteIndex])
        result
    }
}
