/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ext

import org.mozilla.fenix.search.telemetry.SearchProviderModel

fun SearchProviderModel.containsAds(urlList: List<String>): Boolean {
    return urlList.containsAds(this.extraAdServersRegexps)
}

private fun String.isAd(adRegexps: List<String>): Boolean {
    for (adsRegex in adRegexps) {
        if (Regex(adsRegex).containsMatchIn(this)) {
            return true
        }
    }
    return false
}

private fun List<String>.containsAds(adRegexps: List<String>): Boolean {
    for (url in this) {
        if (url.isAd(adRegexps)) {
            return true
        }
    }
    return false
}
