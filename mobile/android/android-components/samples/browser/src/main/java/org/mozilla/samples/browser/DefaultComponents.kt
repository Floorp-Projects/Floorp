/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.samples.browser

import android.content.Context
import android.widget.Toast
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import mozilla.components.browser.domains.autocomplete.ShippedDomainsProvider
import mozilla.components.browser.engine.system.SystemEngine
import mozilla.components.browser.icons.BrowserIcons
import mozilla.components.browser.menu.BrowserMenuBuilder
import mozilla.components.browser.menu.item.BrowserMenuItemToolbar
import mozilla.components.browser.menu.item.BrowserMenuCheckbox
import mozilla.components.browser.menu.item.BrowserMenuDivider
import mozilla.components.browser.menu.item.BrowserMenuImageText
import mozilla.components.browser.menu.item.SimpleBrowserMenuItem
import mozilla.components.browser.search.SearchEngineManager
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.browser.session.storage.SessionStorage
import mozilla.components.browser.storage.memory.InMemoryHistoryStorage
import mozilla.components.concept.engine.DefaultSettings
import mozilla.components.concept.engine.Engine
import mozilla.components.feature.intent.IntentProcessor
import mozilla.components.feature.search.SearchUseCases
import mozilla.components.feature.session.SessionUseCases
import mozilla.components.feature.session.HistoryDelegate
import mozilla.components.feature.tabs.TabsUseCases
import org.mozilla.samples.browser.integration.FindInPageIntegration
import org.mozilla.samples.browser.request.SampleRequestInterceptor
import java.util.concurrent.TimeUnit

open class DefaultComponents(private val applicationContext: Context) {

    // Engine Settings
    val engineSettings by lazy {
        DefaultSettings().apply {
            historyTrackingDelegate = HistoryDelegate(historyStorage)
            requestInterceptor = SampleRequestInterceptor(applicationContext)
            supportMultipleWindows = true
        }
    }

    // Engine
    open val engine: Engine by lazy {
        SystemEngine(applicationContext, engineSettings)
    }

    val icons by lazy { BrowserIcons(applicationContext) }

    // Storage
    val historyStorage by lazy { InMemoryHistoryStorage() }

    private val sessionStorage by lazy { SessionStorage(applicationContext, engine) }

    val sessionManager by lazy {
        SessionManager(engine,
                defaultSession = { Session("about:blank") }
        ).apply {
            sessionStorage.restore()?.let { snapshot -> restore(snapshot) }

            if (size == 0) {
                add(Session("https://www.mozilla.org"))
            }

            sessionStorage.autoSave(this)
                .periodicallyInForeground(interval = 30, unit = TimeUnit.SECONDS)
                .whenGoingToBackground()
                .whenSessionsChange()
        }
    }

    val sessionUseCases by lazy { SessionUseCases(sessionManager) }

    // Search
    val searchEngineManager by lazy {
        SearchEngineManager().apply {
            CoroutineScope(Dispatchers.Default).launch {
                load(applicationContext).await()
            }
        }
    }

    val searchUseCases by lazy { SearchUseCases(applicationContext, searchEngineManager, sessionManager) }
    val defaultSearchUseCase by lazy { { searchTerms: String -> searchUseCases.defaultSearch.invoke(searchTerms) } }

    // Intent
    val sessionIntentProcessor by lazy {
        IntentProcessor(
            sessionUseCases,
            sessionManager,
            searchUseCases,
            applicationContext
        )
    }

    // Menu
    val menuBuilder by lazy { BrowserMenuBuilder(menuItems) }

    private val menuItems by lazy {
        listOf(
                menuToolbar,
                BrowserMenuImageText("Share", R.drawable.mozac_ic_share, android.R.color.black) {
                    Toast.makeText(applicationContext, "Share", Toast.LENGTH_SHORT).show()
                },
                SimpleBrowserMenuItem("Settings") {
                    Toast.makeText(applicationContext, "Settings", Toast.LENGTH_SHORT).show()
                },
                SimpleBrowserMenuItem("Find In Page") {
                    FindInPageIntegration.launch?.invoke()
                },
                BrowserMenuDivider(),
                SimpleBrowserMenuItem("Clear Data") {
                    sessionUseCases.clearData.invoke()
                },
                BrowserMenuCheckbox("Request desktop site", {
                    sessionManager.selectedSessionOrThrow.desktopMode
                }) { checked ->
                    sessionUseCases.requestDesktopSite.invoke(checked)
                }
        )
    }

    private val menuToolbar by lazy {
        val forward = BrowserMenuItemToolbar.Button(
                mozilla.components.ui.icons.R.drawable.mozac_ic_forward,
                iconTintColorResource = R.color.photonBlue90,
                contentDescription = "Forward") {
            sessionUseCases.goForward.invoke()
        }

        val refresh = BrowserMenuItemToolbar.Button(
                mozilla.components.ui.icons.R.drawable.mozac_ic_refresh,
                iconTintColorResource = R.color.photonBlue90,
                contentDescription = "Refresh") {
            sessionUseCases.reload.invoke()
        }

        val stop = BrowserMenuItemToolbar.Button(
                mozilla.components.ui.icons.R.drawable.mozac_ic_stop,
                iconTintColorResource = R.color.photonBlue90,
                contentDescription = "Stop") {
            sessionUseCases.stopLoading.invoke()
        }

        BrowserMenuItemToolbar(listOf(forward, refresh, stop))
    }

    val shippedDomainsProvider by lazy {
        ShippedDomainsProvider().also { it.initialize(applicationContext) }
    }

    // Tabs
    val tabsUseCases: TabsUseCases by lazy { TabsUseCases(sessionManager) }
}
