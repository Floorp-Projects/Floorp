/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.search.provider.localization

/**
 * Class providing language, country and optionally region (actual location) of the user/device to
 * customize the search experience.
 */
interface SearchLocalizationProvider {
    suspend fun determineRegion(): SearchLocalization
}
