/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus

import mozilla.components.browser.search.SearchEngineManager
import mozilla.components.browser.search.provider.AssetsSearchEngineProvider
import mozilla.components.browser.search.provider.localization.LocaleSearchLocalizationProvider
import org.mozilla.focus.search.BingSearchEngineFilter
import org.mozilla.focus.search.CustomSearchEngineProvider
import org.mozilla.focus.search.HiddenSearchEngineFilter

/**
 * Helper object for lazily initializing components.
 */
object Components {
    val searchEngineManager by lazy {
        val assetsProvider = AssetsSearchEngineProvider(
                LocaleSearchLocalizationProvider(),
                filters = listOf(BingSearchEngineFilter(), HiddenSearchEngineFilter()),
                additionalIdentifiers = listOf("ddg"))

        val customProvider = CustomSearchEngineProvider()

        SearchEngineManager(listOf(assetsProvider, customProvider))
    }
}
