/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components

import android.content.Context
import kotlinx.coroutines.Dispatchers.IO
import kotlinx.coroutines.GlobalScope
import kotlinx.coroutines.launch
import mozilla.components.browser.search.SearchEngineManager
import mozilla.components.browser.search.provider.AssetsSearchEngineProvider
import mozilla.components.browser.search.provider.localization.SearchLocalizationProvider
import org.mozilla.fenix.test.Mockable
import org.mozilla.fenix.utils.Settings
import java.util.Locale

// Create our own SearchLocalizationProivder that sets the region based on the country
// This is a temporary fix until we have a localization provider that can look up the users
// issue
// https://github.com/mozilla-mobile/fenix/issues/3869
// https://github.com/mozilla-mobile/android-components/issues/3637
class FenixLocaleSearchLocalizationProvider : SearchLocalizationProvider() {
    override val language: String
        get() = Locale.getDefault().language

    override val country: String
        get() = Locale.getDefault().country

    override val region: String
        get() = country
}

/**
 * Component group for all search engine integration related functionality.
 */
@Mockable
class Search(private val context: Context) {

    /**
     * This component provides access to a centralized registry of search engines.
     */
    val searchEngineManager by lazy {
        SearchEngineManager(coroutineContext = IO, providers = listOf(
            AssetsSearchEngineProvider(FenixLocaleSearchLocalizationProvider())
        )).apply {
            registerForLocaleUpdates(context)
            GlobalScope.launch {
                loadAsync(context).await()
            }
            defaultSearchEngine = getDefaultSearchEngine(
                context,
                Settings.getInstance(context).defaultSearchEngineName
            )
        }
    }
}
