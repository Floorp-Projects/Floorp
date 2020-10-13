/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.search.ext

import android.net.Uri
import mozilla.components.browser.state.search.SearchEngine

/**
 * Converts a [SearchEngine] (from `browser-state`) to the legacy `SearchEngine` type from
 * `browser-search`.
 *
 * This method is a temporary workaround to allow applications to switch to the new API slowly.
 * Once all consuming apps have been migrated this extension will be removed and all components
 * will be migrated to use only the new [SearchEngine] class.
 *
 * https://github.com/mozilla-mobile/android-components/issues/8686
 */
fun SearchEngine.legacy() = mozilla.components.browser.search.SearchEngine(
    identifier = id,
    name = name,
    icon = icon,
    resultsUris = resultUrls.map { Uri.parse(it) },
    suggestUri = suggestUrl?.let { Uri.parse(it) }
)
