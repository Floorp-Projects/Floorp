/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components

import android.content.Context
import mozilla.components.browser.search.SearchEngineManager
import mozilla.components.browser.session.SessionManager
import mozilla.components.concept.engine.Settings
import mozilla.components.feature.app.links.AppLinksUseCases
import mozilla.components.feature.search.SearchUseCases
import mozilla.components.feature.session.SessionUseCases
import mozilla.components.feature.session.SettingsUseCases
import mozilla.components.feature.tabs.TabsUseCases
import org.mozilla.fenix.test.Mockable

/**
 * Component group for all use cases. Use cases are provided by feature
 * modules and can be triggered by UI interactions.
 */
@Mockable
class UseCases(
    private val context: Context,
    private val sessionManager: SessionManager,
    private val engineSettings: Settings,
    private val searchEngineManager: SearchEngineManager
) {
    /**
     * Use cases that provide engine interactions for a given browser session.
     */
    val sessionUseCases by lazy { SessionUseCases(sessionManager) }

    /**
     * Use cases that provide tab management.
     */
    val tabsUseCases: TabsUseCases by lazy { TabsUseCases(sessionManager) }

    /**
     * Use cases that provide search engine integration.
     */
    val searchUseCases by lazy { SearchUseCases(context, searchEngineManager, sessionManager) }

    /**
     * Use cases that provide settings management.
     */
    val settingsUseCases by lazy { SettingsUseCases(engineSettings, sessionManager) }

    val appLinksUseCases by lazy { AppLinksUseCases(context.applicationContext) }
}
