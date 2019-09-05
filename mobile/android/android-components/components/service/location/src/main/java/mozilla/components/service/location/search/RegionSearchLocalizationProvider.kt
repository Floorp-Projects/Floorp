/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.location.search

import mozilla.components.browser.search.provider.localization.SearchLocalization
import mozilla.components.browser.search.provider.localization.SearchLocalizationProvider
import mozilla.components.service.location.MozillaLocationService
import java.util.Locale

/**
 * [SearchLocalizationProvider] implementation that uses a [MozillaLocationService] instance to
 * do a region lookup via GeoIP.
 *
 * Since this provider may do a network request to determine the region, it is important to only use
 * the "async" suspension methods of `SearchEngineManager` from the main thread when using this
 * provider.
 */
class RegionSearchLocalizationProvider(
    private val service: MozillaLocationService
) : SearchLocalizationProvider {
    override suspend fun determineRegion(): SearchLocalization {
        val region = service.fetchRegion(readFromCache = true)

        return SearchLocalization(
            Locale.getDefault().language,
            Locale.getDefault().country,
            region?.countryCode ?: Locale.getDefault().country
        )
    }
}
