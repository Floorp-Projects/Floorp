/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.search.ext

import android.content.Context
import org.mozilla.fenix.components.searchengine.FenixSearchEngineProvider

private const val MINIMUM_SEARCH_ENGINES_NUMBER_TO_SHOW_SHORTCUTS = 2

/**
 * Return if the user has *at least 2* installed search engines.
 * Useful to decide whether to show / enable certain functionalities.
 */
fun FenixSearchEngineProvider.areShortcutsAvailable(context: Context) =
    installedSearchEngines(context).list.size >= MINIMUM_SEARCH_ENGINES_NUMBER_TO_SHOW_SHORTCUTS
