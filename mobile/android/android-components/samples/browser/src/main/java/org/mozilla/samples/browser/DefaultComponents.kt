/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.samples.browser

import android.content.Context
import android.content.Intent
import android.content.SharedPreferences
import android.widget.Toast
import androidx.core.content.ContextCompat
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.MainScope
import kotlinx.coroutines.launch
import mozilla.components.browser.domains.autocomplete.ShippedDomainsProvider
import mozilla.components.browser.engine.system.SystemEngine
import mozilla.components.browser.icons.BrowserIcons
import mozilla.components.browser.menu.BrowserMenuHighlight
import mozilla.components.browser.menu.WebExtensionBrowserMenuBuilder
import mozilla.components.browser.menu.item.BrowserMenuCheckbox
import mozilla.components.browser.menu.item.BrowserMenuDivider
import mozilla.components.browser.menu.item.BrowserMenuHighlightableItem
import mozilla.components.browser.menu.item.BrowserMenuImageText
import mozilla.components.browser.menu.item.BrowserMenuItemToolbar
import mozilla.components.browser.menu.item.SimpleBrowserMenuItem
import mozilla.components.browser.search.SearchEngineManager
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.browser.session.engine.EngineMiddleware
import mozilla.components.browser.session.storage.SessionStorage
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.browser.storage.sync.PlacesHistoryStorage
import mozilla.components.browser.thumbnails.ThumbnailsMiddleware
import mozilla.components.browser.thumbnails.storage.ThumbnailStorage
import mozilla.components.concept.engine.DefaultSettings
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.fetch.Client
import mozilla.components.feature.addons.AddonManager
import mozilla.components.feature.addons.amo.AddonCollectionProvider
import mozilla.components.feature.addons.migration.DefaultSupportedAddonsChecker
import mozilla.components.feature.addons.migration.SupportedAddonsChecker
import mozilla.components.feature.addons.update.AddonUpdater
import mozilla.components.feature.addons.update.DefaultAddonUpdater
import mozilla.components.feature.app.links.AppLinksInterceptor
import mozilla.components.feature.app.links.AppLinksUseCases
import mozilla.components.feature.contextmenu.ContextMenuUseCases
import mozilla.components.feature.customtabs.CustomTabIntentProcessor
import mozilla.components.feature.customtabs.store.CustomTabsServiceStore
import mozilla.components.feature.downloads.DownloadMiddleware
import mozilla.components.feature.downloads.DownloadsUseCases
import mozilla.components.feature.intent.processing.TabIntentProcessor
import mozilla.components.feature.media.RecordingDevicesNotificationFeature
import mozilla.components.feature.media.middleware.MediaMiddleware
import mozilla.components.feature.pwa.ManifestStorage
import mozilla.components.feature.pwa.WebAppInterceptor
import mozilla.components.feature.pwa.WebAppShortcutManager
import mozilla.components.feature.pwa.WebAppUseCases
import mozilla.components.feature.pwa.intent.TrustedWebActivityIntentProcessor
import mozilla.components.feature.pwa.intent.WebAppIntentProcessor
import mozilla.components.feature.readerview.ReaderViewMiddleware
import mozilla.components.feature.search.SearchUseCases
import mozilla.components.feature.session.HistoryDelegate
import mozilla.components.feature.session.SessionUseCases
import mozilla.components.feature.sitepermissions.SitePermissionsStorage
import mozilla.components.feature.tabs.TabsUseCases
import mozilla.components.feature.webnotifications.WebNotificationFeature
import mozilla.components.lib.crash.Crash
import mozilla.components.lib.crash.CrashReporter
import mozilla.components.lib.crash.service.CrashReporterService
import mozilla.components.lib.fetch.httpurlconnection.HttpURLConnectionClient
import mozilla.components.lib.nearby.NearbyConnection
import mozilla.components.service.digitalassetlinks.local.StatementApi
import mozilla.components.service.digitalassetlinks.local.StatementRelationChecker
import mozilla.components.support.base.crash.Breadcrumb
import org.mozilla.samples.browser.addons.AddonsActivity
import org.mozilla.samples.browser.downloads.DownloadService
import org.mozilla.samples.browser.ext.components
import org.mozilla.samples.browser.integration.FindInPageIntegration
import org.mozilla.samples.browser.integration.P2PIntegration
import org.mozilla.samples.browser.media.MediaService
import org.mozilla.samples.browser.request.SampleRequestInterceptor
import java.util.concurrent.TimeUnit

private const val DAY_IN_MINUTES = 24 * 60L

@Suppress("LargeClass")
open class DefaultComponents(private val applicationContext: Context) {
    companion object {
        const val SAMPLE_BROWSER_PREFERENCES = "sample_browser_preferences"
        const val PREF_LAUNCH_EXTERNAL_APP = "sample_browser_launch_external_app"
    }

    val preferences: SharedPreferences =
        applicationContext.getSharedPreferences(SAMPLE_BROWSER_PREFERENCES, Context.MODE_PRIVATE)

    // Engine Settings
    val engineSettings by lazy {
        DefaultSettings().apply {
            historyTrackingDelegate = HistoryDelegate(lazyHistoryStorage)
            requestInterceptor = SampleRequestInterceptor(applicationContext)
            remoteDebuggingEnabled = true
            supportMultipleWindows = true
        }
    }

    val addonUpdater =
        DefaultAddonUpdater(applicationContext, AddonUpdater.Frequency(1, TimeUnit.DAYS))

    // Engine
    open val engine: Engine by lazy {
        SystemEngine(applicationContext, engineSettings)
    }

    open val client: Client by lazy { HttpURLConnectionClient() }

    val icons by lazy { BrowserIcons(applicationContext, client) }

    // Storage
    private val lazyHistoryStorage = lazy { PlacesHistoryStorage(applicationContext) }
    val historyStorage by lazy { lazyHistoryStorage.value }

    private val sessionStorage by lazy { SessionStorage(applicationContext, engine) }

    val permissionStorage by lazy { SitePermissionsStorage(applicationContext) }

    val thumbnailStorage by lazy { ThumbnailStorage(applicationContext) }

    val store by lazy {
        BrowserStore(middleware = listOf(
            MediaMiddleware(applicationContext, MediaService::class.java),
            DownloadMiddleware(applicationContext, DownloadService::class.java),
            ReaderViewMiddleware(),
            ThumbnailsMiddleware(thumbnailStorage)
        ) + EngineMiddleware.create(engine, ::findSessionById))
    }

    val customTabsStore by lazy { CustomTabsServiceStore() }

    private fun findSessionById(tabId: String): Session? {
        return sessionManager.findSessionById(tabId)
    }

    val sessionManager by lazy {
        SessionManager(engine, store).apply {
            sessionStorage.restore()?.let {
                snapshot -> restore(snapshot)
            }

            if (size == 0) {
                add(Session("about:blank"))
            }

            sessionStorage.autoSave(store)
                .periodicallyInForeground(interval = 30, unit = TimeUnit.SECONDS)
                .whenGoingToBackground()
                .whenSessionsChange()

            icons.install(engine, store)

            RecordingDevicesNotificationFeature(applicationContext, sessionManager = this)
                .enable()

            WebNotificationFeature(applicationContext, engine, icons, R.drawable.ic_notification,
                permissionStorage, BrowserActivity::class.java)
        }
    }

    val sessionUseCases by lazy { SessionUseCases(store, sessionManager) }

    // Addons
    val addonManager by lazy {
        AddonManager(store, engine, addonCollectionProvider, addonUpdater)
    }

    val addonCollectionProvider by lazy {
        AddonCollectionProvider(
            applicationContext,
            client,
            collectionName = "3204bb44a6ef44d39ee34917f28055",
            maxCacheAgeInMinutes = DAY_IN_MINUTES
        )
    }

    val supportedAddonsChecker by lazy {
        DefaultSupportedAddonsChecker(applicationContext, SupportedAddonsChecker.Frequency(1, TimeUnit.DAYS))
    }

    // Search
    val searchEngineManager by lazy {
        SearchEngineManager().apply {
            CoroutineScope(Dispatchers.IO).launch {
                loadAsync(applicationContext).await()
            }
        }
    }

    val searchUseCases by lazy { SearchUseCases(applicationContext, store, searchEngineManager, sessionManager) }
    val defaultSearchUseCase by lazy {
        { searchTerms: String ->
            searchUseCases.defaultSearch.invoke(
                searchTerms = searchTerms,
                searchEngine = null,
                parentSession = null
            )
        }
    }
    val appLinksUseCases by lazy { AppLinksUseCases(applicationContext) }

    val appLinksInterceptor by lazy {
        AppLinksInterceptor(
            applicationContext,
            interceptLinkClicks = true,
            launchInApp = {
                applicationContext.components.preferences.getBoolean(PREF_LAUNCH_EXTERNAL_APP, false)
            },
            launchFromInterceptor = true
        )
    }

    val webAppInterceptor by lazy {
        WebAppInterceptor(
            applicationContext,
            webAppManifestStorage
        )
    }

    val webAppManifestStorage by lazy { ManifestStorage(applicationContext) }
    val webAppShortcutManager by lazy { WebAppShortcutManager(applicationContext, client, webAppManifestStorage) }
    val webAppUseCases by lazy { WebAppUseCases(applicationContext, sessionManager, webAppShortcutManager) }

    // P2P communication
    val nearbyConnection by lazy {
        NearbyConnection(applicationContext)
    }

    // Digital Asset Links checking
    val relationChecker by lazy {
        StatementRelationChecker(StatementApi(client))
    }

    // Intent
    val tabIntentProcessor by lazy {
        TabIntentProcessor(sessionManager, sessionUseCases.loadUrl, searchUseCases.newTabSearch)
    }
    val externalAppIntentProcessors by lazy {
        listOf(
            WebAppIntentProcessor(sessionManager, sessionUseCases.loadUrl, webAppManifestStorage),
            TrustedWebActivityIntentProcessor(
                sessionManager,
                sessionUseCases.loadUrl,
                applicationContext.packageManager,
                relationChecker,
                customTabsStore
            ),
            CustomTabIntentProcessor(sessionManager, sessionUseCases.loadUrl, applicationContext.resources)
        )
    }

    // Menu
    val menuBuilder by lazy {
        WebExtensionBrowserMenuBuilder(
            menuItems,
            store = store,
            webExtIconTintColorResource = R.color.photonGrey90,
            onAddonsManagerTapped = {
                val intent = Intent(applicationContext, AddonsActivity::class.java)
                intent.flags = Intent.FLAG_ACTIVITY_NEW_TASK
                applicationContext.startActivity(intent)
            }
        )
    }

    private val menuItems by lazy {
        val items = mutableListOf(
            menuToolbar,
            BrowserMenuHighlightableItem("No Highlight", R.drawable.mozac_ic_share, android.R.color.black,
                highlight = BrowserMenuHighlight.LowPriority(
                    notificationTint = ContextCompat.getColor(applicationContext, android.R.color.holo_green_dark),
                    label = "Highlight"
                )
            ) {
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
            SimpleBrowserMenuItem("P2P") {
                P2PIntegration.launch?.invoke()
            },
            SimpleBrowserMenuItem("Restore after crash") {
                sessionUseCases.crashRecovery.invoke()
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
            SimpleBrowserMenuItem("Open in App") {
                val getRedirect = appLinksUseCases.appLinkRedirect
                sessionManager.selectedSession?.let {
                    val redirect = getRedirect.invoke(it.url)
                    redirect.appIntent?.flags = Intent.FLAG_ACTIVITY_NEW_TASK
                    appLinksUseCases.openAppLink.invoke(redirect.appIntent)
                }
            }.apply {
                visible = {
                    sessionManager.selectedSession?.let {
                        appLinksUseCases.appLinkRedirect(it.url).hasExternalApp()
                    } ?: false
                }
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
            }.apply {
                visible = { sessionManager.selectedSession != null }
            }
        )
        items.add(
            BrowserMenuCheckbox("Open links in apps", {
                preferences.getBoolean(PREF_LAUNCH_EXTERNAL_APP, false)
            }) { checked ->
                preferences.edit().putBoolean(PREF_LAUNCH_EXTERNAL_APP, checked).apply()
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
            contentDescription = "Stop"
        ) {
            sessionUseCases.stopLoading()
        }

        BrowserMenuItemToolbar(listOf(forward, refresh, stop))
    }

    val shippedDomainsProvider by lazy {
        ShippedDomainsProvider().also { it.initialize(applicationContext) }
    }

    val tabsUseCases: TabsUseCases by lazy { TabsUseCases(store, sessionManager) }
    val downloadsUseCases: DownloadsUseCases by lazy { DownloadsUseCases(store) }
    val contextMenuUseCases: ContextMenuUseCases by lazy { ContextMenuUseCases(store) }

    val crashReporter: CrashReporter by lazy {
        CrashReporter(
            applicationContext,
            services = listOf(
                object : CrashReporterService {
                    override val id: String
                        get() = "xxx"
                    override val name: String
                        get() = "Test"

                    override fun createCrashReportUrl(identifier: String): String? {
                        return null
                    }

                    override fun report(crash: Crash.UncaughtExceptionCrash): String? {
                        return null
                    }

                    override fun report(crash: Crash.NativeCodeCrash): String? {
                        return null
                    }

                    override fun report(
                        throwable: Throwable,
                        breadcrumbs: ArrayList<Breadcrumb>
                    ): String? {
                        return null
                    }
                }
            )
        ).install(applicationContext)
    }
}
