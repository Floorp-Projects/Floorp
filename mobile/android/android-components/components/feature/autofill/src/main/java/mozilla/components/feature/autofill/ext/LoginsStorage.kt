/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.autofill.ext

import mozilla.components.concept.storage.Login
import mozilla.components.concept.storage.LoginsStorage
import mozilla.components.feature.autofill.structure.ParsedStructure
import mozilla.components.lib.publicsuffixlist.PublicSuffixList
import mozilla.components.support.utils.Browsers

/**
 * Returns [Login]s matching the domain or package name found in the [ParsedStructure].
 */
internal suspend fun LoginsStorage.getByStructure(
    publicSuffixList: PublicSuffixList,
    structure: ParsedStructure
): List<Login> {
    val domain = structure.getLookupDomain(publicSuffixList)
    return getByBaseDomain(domain)
}

/**
 * Try to find a domain in the [ParsedStructure] for looking up logins. This is either a "web domain"
 * for web content the third-party app is displaying (e.g. in a WebView) or the package name of the
 * application transformed into a domain. In any case the [publicSuffixList] will be used to turn
 * the domain into a "base" domain (public suffix + 1) before returning.
 */
private suspend fun ParsedStructure.getLookupDomain(publicSuffixList: PublicSuffixList): String {
    val domain = if (webDomain != null && Browsers.isBrowser(packageName)) {
        // If the application we are auto-filling is a known browser and it provided a webDomain
        // for the content it is displaying then we try to autofill for that.
        webDomain
    } else {
        // We reverse the package name in the hope that this will resemble a domain name. This is
        // of course fragile. So we want to find better mechanisms in the future (e.g. looking up
        // what URLs the application registers intent handlers for).
        packageName.split('.').asReversed().joinToString(".")
    }

    return publicSuffixList.getPublicSuffixPlusOne(domain).await() ?: domain
}
