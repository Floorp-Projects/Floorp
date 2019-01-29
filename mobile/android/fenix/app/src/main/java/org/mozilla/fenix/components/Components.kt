/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components

import android.content.Context
import org.mozilla.fenix.components.toolbar.Toolbar

/**
 * Provides access to all components.
 */
class Components(private val context: Context) {
    val core by lazy { Core(context) }
    val search by lazy { Search(context) }
    val useCases by lazy { UseCases(context, core.sessionManager, search.searchEngineManager) }
    val toolbar by lazy {
        Toolbar(
            context,
            useCases.sessionUseCases,
            core.sessionManager
        )
    }
    val utils by lazy { Utilities(context, core.sessionManager, useCases.sessionUseCases, useCases.searchUseCases) }
}