/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components

import android.content.Context
import mozilla.components.lib.publicsuffixlist.PublicSuffixList
import org.mozilla.fenix.test.Mockable
import org.mozilla.fenix.utils.ClipboardHandler

/**
 * Provides access to all components.
 */
@Mockable
class Components(private val context: Context) {
    val backgroundServices by lazy {
        BackgroundServices(
            context,
            analytics.crashReporter,
            core.historyStorage,
            core.bookmarksStorage,
            core.passwordsStorage,
            core.secureAbove22Preferences
        )
    }
    val services by lazy { Services(context, backgroundServices.accountManager) }
    val core by lazy { Core(context) }
    val search by lazy { Search(context) }
    val useCases by lazy {
        UseCases(
            context,
            core.sessionManager,
            core.store,
            core.engine.settings,
            search.searchEngineManager,
            core.webAppShortcutManager
        )
    }
    val intentProcessors by lazy {
        IntentProcessors(
            context,
            core.sessionManager,
            useCases.sessionUseCases,
            useCases.searchUseCases,
            core.client,
            core.customTabsStore
        )
    }
    val analytics by lazy { Analytics(context) }
    val publicSuffixList by lazy { PublicSuffixList(context) }
    val clipboardHandler by lazy { ClipboardHandler(context) }
}
