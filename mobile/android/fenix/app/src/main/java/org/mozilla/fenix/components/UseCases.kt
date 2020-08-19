/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components

import android.content.Context
import mozilla.components.browser.search.SearchEngineManager
import mozilla.components.browser.session.SessionManager
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.Engine
import mozilla.components.feature.app.links.AppLinksUseCases
import mozilla.components.feature.contextmenu.ContextMenuUseCases
import mozilla.components.feature.downloads.DownloadsUseCases
import mozilla.components.feature.pwa.WebAppShortcutManager
import mozilla.components.feature.pwa.WebAppUseCases
import mozilla.components.feature.search.SearchUseCases
import mozilla.components.feature.session.SessionUseCases
import mozilla.components.feature.session.SettingsUseCases
import mozilla.components.feature.session.TrackingProtectionUseCases
import mozilla.components.feature.tabs.TabsUseCases
import mozilla.components.feature.top.sites.TopSitesStorage
import mozilla.components.feature.top.sites.TopSitesUseCases
import org.mozilla.fenix.utils.Mockable

/**
 * Component group for all use cases. Use cases are provided by feature
 * modules and can be triggered by UI interactions.
 */
@Mockable
@Suppress("LongParameterList")
class UseCases(
    private val context: Context,
    private val engine: Engine,
    private val sessionManager: SessionManager,
    private val store: BrowserStore,
    private val searchEngineManager: SearchEngineManager,
    private val shortcutManager: WebAppShortcutManager,
    private val topSitesStorage: TopSitesStorage
) {
    /**
     * Use cases that provide engine interactions for a given browser session.
     */
    val sessionUseCases by lazy { SessionUseCases(store, sessionManager) }

    /**
     * Use cases that provide tab management.
     */
    val tabsUseCases: TabsUseCases by lazy { TabsUseCases(store, sessionManager) }

    /**
     * Use cases that provide search engine integration.
     */
    val searchUseCases by lazy { SearchUseCases(context, store, searchEngineManager, sessionManager) }

    /**
     * Use cases that provide settings management.
     */
    val settingsUseCases by lazy { SettingsUseCases(engine, store) }

    val appLinksUseCases by lazy { AppLinksUseCases(context.applicationContext) }

    val webAppUseCases by lazy {
        WebAppUseCases(context, sessionManager, shortcutManager)
    }

    val downloadUseCases by lazy { DownloadsUseCases(store) }

    val contextMenuUseCases by lazy { ContextMenuUseCases(store) }

    val trackingProtectionUseCases by lazy { TrackingProtectionUseCases(store, engine) }

    /**
     * Use cases that provide top sites management.
     */
    val topSitesUseCase by lazy { TopSitesUseCases(topSitesStorage) }
}
