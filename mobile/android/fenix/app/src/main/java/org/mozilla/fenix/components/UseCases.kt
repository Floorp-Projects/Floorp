/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components

import android.content.Context
import mozilla.components.browser.search.SearchEngineManager
import mozilla.components.browser.session.SessionManager
import mozilla.components.feature.search.SearchUseCases
import mozilla.components.feature.session.SessionUseCases
import mozilla.components.feature.tabs.TabsUseCases

/**
 * Component group for all use cases. Use cases are provided by feature
 * modules and can be triggered by UI interactions.
 */
class UseCases(
    private val context: Context,
    private val sessionManager: SessionManager,
    private val searchEngineManager: SearchEngineManager
) {

    val searchUseCases by lazy { SearchUseCases(context, searchEngineManager, sessionManager) }
    val sessionUseCases by lazy { SessionUseCases(sessionManager) };
    val tabsUseCases by lazy { TabsUseCases(sessionManager) }
}
