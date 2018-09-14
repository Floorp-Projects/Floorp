/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.samples.browser

import android.content.Context
import android.widget.Toast
import kotlinx.coroutines.experimental.async
import mozilla.components.browser.engine.system.SystemEngine
import mozilla.components.browser.menu.BrowserMenuBuilder
import mozilla.components.browser.menu.item.BrowserMenuItemToolbar
import mozilla.components.browser.menu.item.SimpleBrowserMenuCheckbox
import mozilla.components.browser.menu.item.SimpleBrowserMenuItem
import mozilla.components.browser.search.SearchEngineManager
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.browser.session.storage.DefaultSessionStorage
import mozilla.components.concept.engine.Engine
import mozilla.components.feature.search.SearchUseCases
import mozilla.components.feature.session.SessionIntentProcessor
import mozilla.components.feature.session.SessionUseCases
import mozilla.components.feature.session.TextSearchHandler
import mozilla.components.feature.tabs.TabsUseCases

open class DefaultComponents(private val applicationContext: Context) {

    // Engine
   open val engine: Engine by lazy {
        SystemEngine()
    }

    // Session
    val sessionStorage by lazy { DefaultSessionStorage(applicationContext) }

    val sessionManager by lazy {
        SessionManager(engine).apply {
            sessionStorage.restore(this)

            if (size == 0) {
                val initialSession = Session("https://www.mozilla.org")
                add(initialSession)
            }
        }
    }

    val sessionUseCases by lazy { SessionUseCases(sessionManager) }
    val sessionIntentProcessor by lazy {
        SessionIntentProcessor(
                sessionUseCases, sessionManager,
                textSearchHandler = textSearchHandler
        )
    }

    private val textSearchHandler by lazy {
        val handler: TextSearchHandler = { searchTerm, session ->
            searchUseCases.defaultSearch.invoke(searchTerm, session)
        }
        handler
    }

    // Search
    private val searchEngineManager by lazy {
        SearchEngineManager().apply {
            async { load(applicationContext) }
        }
    }

    private val searchUseCases by lazy { SearchUseCases(applicationContext, searchEngineManager, sessionManager) }
    val defaultSearchUseCase by lazy { { searchTerms: String -> searchUseCases.defaultSearch.invoke(searchTerms) } }

    // Menu
    val menuBuilder by lazy { BrowserMenuBuilder(menuItems) }

    private val menuItems by lazy {
        listOf(
                menuToolbar,
                SimpleBrowserMenuItem("Share") {
                    Toast.makeText(applicationContext, "Share", Toast.LENGTH_SHORT).show()
                },
                SimpleBrowserMenuItem("Settings") {
                    Toast.makeText(applicationContext, "Settings", Toast.LENGTH_SHORT).show()
                },
                SimpleBrowserMenuItem("Clear Data") {
                    sessionUseCases.clearData.invoke()
                },
                SimpleBrowserMenuCheckbox("Request desktop site") { checked ->
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

    // Tabs
    val tabsUseCases: TabsUseCases by lazy { TabsUseCases(sessionManager) }
}