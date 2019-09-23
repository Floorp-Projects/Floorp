/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.samples.browser

import android.content.Context
import android.widget.Toast
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.MainScope
import kotlinx.coroutines.launch
import mozilla.components.browser.domains.autocomplete.ShippedDomainsProvider
import mozilla.components.browser.engine.system.SystemEngine
import mozilla.components.browser.icons.BrowserIcons
import mozilla.components.browser.menu.BrowserMenuBuilder
import mozilla.components.browser.menu.item.BrowserMenuCheckbox
import mozilla.components.browser.menu.item.BrowserMenuDivider
import mozilla.components.browser.menu.item.BrowserMenuHighlightableItem
import mozilla.components.browser.menu.item.BrowserMenuImageText
import mozilla.components.browser.menu.item.BrowserMenuItemToolbar
import mozilla.components.browser.menu.item.SimpleBrowserMenuItem
import mozilla.components.browser.search.SearchEngineManager
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.browser.session.storage.SessionStorage
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.browser.storage.memory.InMemoryHistoryStorage
import mozilla.components.concept.engine.DefaultSettings
import mozilla.components.concept.engine.Engine
import mozilla.components.feature.contextmenu.ContextMenuUseCases
import mozilla.components.feature.customtabs.CustomTabIntentProcessor
import mozilla.components.feature.customtabs.store.CustomTabsServiceStore
import mozilla.components.feature.downloads.DownloadsUseCases
import mozilla.components.feature.intent.processing.TabIntentProcessor
import mozilla.components.feature.media.MediaFeature
import mozilla.components.feature.media.RecordingDevicesNotificationFeature
import mozilla.components.feature.media.state.MediaStateMachine
import mozilla.components.feature.pwa.ManifestStorage
import mozilla.components.feature.pwa.WebAppUseCases
import mozilla.components.feature.pwa.intent.TrustedWebActivityIntentProcessor
import mozilla.components.feature.pwa.intent.WebAppIntentProcessor
import mozilla.components.feature.search.SearchUseCases
import mozilla.components.feature.session.HistoryDelegate
import mozilla.components.feature.session.SessionUseCases
import mozilla.components.feature.tabs.TabsUseCases
import mozilla.components.lib.fetch.httpurlconnection.HttpURLConnectionClient
import org.mozilla.samples.browser.integration.FindInPageIntegration
import org.mozilla.samples.browser.request.SampleRequestInterceptor
import java.util.concurrent.TimeUnit

open class DefaultComponents(private val applicationContext: Context) {

    // Engine Settings
    val engineSettings by lazy {
        DefaultSettings().apply {
            historyTrackingDelegate = HistoryDelegate(historyStorage)
            requestInterceptor = SampleRequestInterceptor(applicationContext)
            remoteDebuggingEnabled = true
            supportMultipleWindows = true
        }
    }

    // Engine
    open val engine: Engine by lazy {
        SystemEngine(applicationContext, engineSettings)
    }

    val client by lazy { HttpURLConnectionClient() }

    val icons by lazy { BrowserIcons(applicationContext, client) }

    // Storage
    val historyStorage by lazy { InMemoryHistoryStorage() }

    private val sessionStorage by lazy { SessionStorage(applicationContext, engine) }

    val store by lazy { BrowserStore() }

    val customTabsStore by lazy { CustomTabsServiceStore() }

    val sessionManager by lazy {
        SessionManager(engine, store).apply {
            sessionStorage.restore()?.let { snapshot -> restore(snapshot) }

            if (size == 0) {
                add(Session("about:blank"))
            }

            sessionStorage.autoSave(this)
                .periodicallyInForeground(interval = 30, unit = TimeUnit.SECONDS)
                .whenGoingToBackground()
                .whenSessionsChange()

            icons.install(engine, this)

            RecordingDevicesNotificationFeature(applicationContext, sessionManager = this)
                .enable()

            MediaFeature(applicationContext)
                .enable()

            MediaStateMachine.start(this)
        }
    }

    val sessionUseCases by lazy { SessionUseCases(sessionManager) }

    // Search
    val searchEngineManager by lazy {
        SearchEngineManager().apply {
            CoroutineScope(Dispatchers.IO).launch {
                loadAsync(applicationContext).await()
            }
        }
    }

    val searchUseCases by lazy { SearchUseCases(applicationContext, searchEngineManager, sessionManager) }
    val defaultSearchUseCase by lazy { { searchTerms: String -> searchUseCases.defaultSearch.invoke(searchTerms) } }

    val webAppUseCases by lazy { WebAppUseCases(applicationContext, sessionManager, client) }

    // Intent
    val tabIntentProcessor by lazy {
        TabIntentProcessor(sessionManager, sessionUseCases.loadUrl, searchUseCases.newTabSearch)
    }
    val externalAppIntentProcessors by lazy {
        listOf(
            WebAppIntentProcessor(sessionManager, sessionUseCases.loadUrl, ManifestStorage(applicationContext)),
            TrustedWebActivityIntentProcessor(
                sessionManager,
                sessionUseCases.loadUrl,
                client,
                applicationContext.packageManager,
                null,
                customTabsStore
            ),
            CustomTabIntentProcessor(sessionManager, sessionUseCases.loadUrl, applicationContext.resources)
        )
    }

    // Menu
    val menuBuilder by lazy { BrowserMenuBuilder(menuItems) }

    private val menuItems by lazy {
        val items = mutableListOf(
            menuToolbar,
                BrowserMenuHighlightableItem("Highlight", R.drawable.mozac_ic_share, android.R.color.black, highlight =
                BrowserMenuHighlightableItem.Highlight(
                    R.drawable.mozac_ic_search, R.drawable.mozac_ic_stop,
                    R.drawable.background_with_ripple,
                    android.R.color.holo_green_dark
                )) {
                    Toast.makeText(applicationContext, "Highlight", Toast.LENGTH_SHORT).show()
                },
            BrowserMenuImageText("Share", R.drawable.mozac_ic_share, android.R.color.black) {
                Toast.makeText(applicationContext, "Share", Toast.LENGTH_SHORT).show()
            },
            SimpleBrowserMenuItem("Settings") {
                Toast.makeText(applicationContext, "Settings", Toast.LENGTH_SHORT).show()
            },
            SimpleBrowserMenuItem("Find In Page") {
                FindInPageIntegration.launch?.invoke()
            },
            BrowserMenuDivider()
        )

        items.add(
            SimpleBrowserMenuItem("Add to homescreen") {
                MainScope().launch {
                    webAppUseCases.addToHomescreen()
                }
            }.apply {
                visible = { webAppUseCases.isPinningSupported() && sessionManager.selectedSession != null }
            }
        )

        items.add(
            SimpleBrowserMenuItem("Clear Data") {
                sessionUseCases.clearData()
            }
        )
        items.add(
            BrowserMenuCheckbox("Request desktop site", {
                sessionManager.selectedSessionOrThrow.desktopMode
            }) { checked ->
                sessionUseCases.requestDesktopSite(checked)
            }
        )

        items
    }

    private val menuToolbar by lazy {
        val forward = BrowserMenuItemToolbar.Button(
                mozilla.components.ui.icons.R.drawable.mozac_ic_forward,
                iconTintColorResource = R.color.photonBlue90,
                contentDescription = "Forward",
                isEnabled = { sessionManager.selectedSession?.canGoForward == true }
        ) {
            sessionUseCases.goForward()
        }

        val refresh = BrowserMenuItemToolbar.Button(
                mozilla.components.ui.icons.R.drawable.mozac_ic_refresh,
                iconTintColorResource = R.color.photonBlue90,
                contentDescription = "Refresh",
                isEnabled = { sessionManager.selectedSession?.loading != true }
        ) {
            sessionUseCases.reload()
        }

        val stop = BrowserMenuItemToolbar.Button(
                mozilla.components.ui.icons.R.drawable.mozac_ic_stop,
                iconTintColorResource = R.color.photonBlue90,
                contentDescription = "Stop") {
            sessionUseCases.stopLoading()
        }

        BrowserMenuItemToolbar(listOf(forward, refresh, stop))
    }

    val shippedDomainsProvider by lazy {
        ShippedDomainsProvider().also { it.initialize(applicationContext) }
    }

    val tabsUseCases: TabsUseCases by lazy { TabsUseCases(sessionManager) }
    val downloadsUseCases: DownloadsUseCases by lazy { DownloadsUseCases(store) }
    val contextMenuUseCases: ContextMenuUseCases by lazy { ContextMenuUseCases(sessionManager, store) }
}
