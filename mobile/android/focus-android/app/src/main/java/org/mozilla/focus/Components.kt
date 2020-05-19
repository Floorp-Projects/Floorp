/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus

import android.content.Context
import mozilla.components.browser.search.SearchEngineManager
import mozilla.components.browser.search.provider.AssetsSearchEngineProvider
import mozilla.components.browser.search.provider.localization.LocaleSearchLocalizationProvider
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.browser.session.engine.EngineMiddleware
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.DefaultSettings
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.fetch.Client
import mozilla.components.feature.contextmenu.ContextMenuUseCases
import mozilla.components.feature.downloads.DownloadMiddleware
import mozilla.components.feature.downloads.DownloadsUseCases
import mozilla.components.feature.session.SessionUseCases
import mozilla.components.feature.session.SettingsUseCases
import mozilla.components.feature.session.TrackingProtectionUseCases
import mozilla.components.feature.tabs.TabsUseCases
import org.mozilla.focus.components.EngineProvider
import org.mozilla.focus.downloads.DownloadService
import org.mozilla.focus.engine.LocalizedContentInterceptor
import org.mozilla.focus.search.BingSearchEngineFilter
import org.mozilla.focus.search.CustomSearchEngineProvider
import org.mozilla.focus.search.HiddenSearchEngineFilter
import org.mozilla.focus.utils.Settings

/**
 * Helper object for lazily initializing components.
 */
class Components(
    context: Context
) {
    val engineDefaultSettings by lazy {
        val settings = Settings.getInstance(context)

        DefaultSettings(
                requestInterceptor = LocalizedContentInterceptor(context),
                trackingProtectionPolicy = settings.createTrackingProtectionPolicy(),
                javascriptEnabled = !settings.shouldBlockJavaScript(),
                remoteDebuggingEnabled = settings.shouldEnableRemoteDebugging(),
                webFontsEnabled = !settings.shouldBlockWebFonts()
        )
    }

    val engine: Engine by lazy {
        EngineProvider.createEngine(context, engineDefaultSettings).apply {
            Settings.getInstance(context).setupSafeBrowsing(this)
        }
    }

    val client: Client by lazy { EngineProvider.createClient(context) }

    val trackingProtectionUseCases by lazy { TrackingProtectionUseCases(store, engine) }

    val settingsUseCases by lazy { SettingsUseCases(engine, store) }

    val store by lazy {
        BrowserStore(
            middleware = listOf(
                DownloadMiddleware(context, DownloadService::class.java)
            ) + EngineMiddleware.create(engine, ::findSessionById)
        )
    }

    private fun findSessionById(tabId: String): Session? {
        return sessionManager.findSessionById(tabId)
    }

    val sessionUseCases: SessionUseCases by lazy { SessionUseCases(store, sessionManager) }

    val tabsUseCases: TabsUseCases by lazy { TabsUseCases(store, sessionManager) }

    val contextMenuUseCases: ContextMenuUseCases by lazy { ContextMenuUseCases(store) }

    val downloadsUseCases: DownloadsUseCases by lazy { DownloadsUseCases(store) }

    val sessionManager by lazy {
        SessionManager(engine, store)
    }

    val searchEngineManager by lazy {
        val assetsProvider = AssetsSearchEngineProvider(
                LocaleSearchLocalizationProvider(),
                filters = listOf(BingSearchEngineFilter(), HiddenSearchEngineFilter()),
                additionalIdentifiers = listOf("ddg"))

        val customProvider = CustomSearchEngineProvider()

        SearchEngineManager(listOf(assetsProvider, customProvider))
    }
}
