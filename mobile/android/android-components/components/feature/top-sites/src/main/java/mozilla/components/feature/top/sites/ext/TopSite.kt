/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.top.sites.ext

import mozilla.components.feature.top.sites.TopSite

/**
 * Returns true if the given url is in the list top site and false otherwise.
 *
 * @param url The URL string.
 */
fun List<TopSite>.hasUrl(url: String): Boolean {
    for (topSite in this) {
        if (topSite.url == url) {
            return true
        }
    }

    return false
}
