package org.mozilla.fenix.ext

import org.mozilla.fenix.ads.SearchProviderModel

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
