/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.top.sites.ext

import mozilla.components.concept.storage.TopFrecentSiteInfo
import mozilla.components.feature.top.sites.TopSite
import mozilla.components.support.ktx.kotlin.tryGetHostFromUrl

/**
 * Returns a [TopSite] for the given [TopFrecentSiteInfo].
 */
fun TopFrecentSiteInfo.toTopSite(): TopSite {
    return TopSite.Frecent(
        id = null,
        title = this.title?.takeIf(String::isNotBlank) ?: this.url.tryGetHostFromUrl(),
        url = this.url,
        createdAt = null,
    )
}
