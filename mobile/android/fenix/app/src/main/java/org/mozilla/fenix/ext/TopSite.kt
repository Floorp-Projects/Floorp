/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ext

import android.net.Uri
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

/**
 * Returns true if the url contains any query parameters specified by the [searchParameters].
 *
 * @param searchParameters [String] of the following forms:
 * - "" (empty) - Don't search for any params
 * - "key" - Search param named "key" with any or no value
 * - "key=" - Search param named "key" with no value
 * - "key=value" - Search param named "key" with value "value"
 */
fun TopSite.containsQueryParameters(searchParameters: String): Boolean {
    if (searchParameters.isBlank()) {
        return false
    }
    val params = searchParameters.split("=")
    val uri = Uri.parse(url)
    return when (params.size) {
        1 -> {
            uri.queryParameterNames.contains(params.first()) &&
                uri.getQueryParameter(params.first()).isNullOrBlank()
        }
        2 -> {
            uri.queryParameterNames.contains(params.first()) &&
                uri.getQueryParameter(params.first()) == params.last()
        }
        else -> false
    }
}
