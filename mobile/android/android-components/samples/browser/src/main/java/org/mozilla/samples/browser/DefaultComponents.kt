/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.samples.browser

import android.annotation.SuppressLint
import android.content.Context
import android.content.Intent
import android.content.SharedPreferences
import android.widget.Toast
import androidx.core.content.ContextCompat
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
import mozilla.components.browser.session.storage.SessionStorage
import mozilla.components.browser.state.engine.EngineMiddleware
import mozilla.components.browser.state.engine.middleware.SessionPrioritizationMiddleware
import mozilla.components.browser.state.selector.selectedTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.browser.storage.sync.PlacesHistoryStorage
import mozilla.components.browser.thumbnails.ThumbnailsMiddleware
import mozilla.components.browser.thumbnails.storage.ThumbnailStorage
import mozilla.components.concept.base.crash.Breadcrumb
import mozilla.components.concept.engine.DefaultSettings
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.mediaquery.PreferredColorScheme
import mozilla.components.concept.fetch.Client
import mozilla.components.feature.addons.AddonManager
import mozilla.components.feature.addons.amo.AddonCollectionProvider
import mozilla.components.feature.addons.migration.DefaultSupportedAddonsChecker
import mozilla.components.feature.addons.update.DefaultAddonUpdater
import mozilla.components.feature.app.links.AppLinksInterceptor
import mozilla.components.feature.app.links.AppLinksUseCases
import mozilla.components.feature.autofill.AutofillConfiguration
import mozilla.components.feature.contextmenu.ContextMenuUseCases
import mozilla.components.feature.customtabs.CustomTabIntentProcessor
import mozilla.components.feature.customtabs.store.CustomTabsServiceStore
import mozilla.components.feature.downloads.DownloadMiddleware
import mozilla.components.feature.downloads.DownloadsUseCases
import mozilla.components.feature.intent.processing.TabIntentProcessor
import mozilla.components.feature.media.middleware.RecordingDevicesMiddleware
import mozilla.components.feature.prompts.PromptMiddleware
import mozilla.components.feature.pwa.ManifestStorage
import mozilla.components.feature.pwa.WebAppInterceptor
import mozilla.components.feature.pwa.WebAppShortcutManager
import mozilla.components.feature.pwa.WebAppUseCases
import mozilla.components.feature.pwa.intent.WebAppIntentProcessor
import mozilla.components.feature.readerview.ReaderViewMiddleware
import mozilla.components.feature.search.SearchUseCases
import mozilla.components.feature.search.middleware.SearchMiddleware
import mozilla.components.feature.search.region.RegionMiddleware
import mozilla.components.feature.session.HistoryDelegate
import mozilla.components.feature.session.SessionUseCases
import mozilla.components.feature.session.middleware.LastAccessMiddleware
import mozilla.components.feature.session.middleware.undo.UndoMiddleware
import mozilla.components.feature.sitepermissions.OnDiskSitePermissionsStorage
import mozilla.components.feature.tabs.CustomTabsUseCases
import mozilla.components.feature.tabs.TabsUseCases
import mozilla.components.lib.crash.Crash
import mozilla.components.lib.crash.CrashReporter
import mozilla.components.lib.crash.service.CrashReporterService
import mozilla.components.lib.dataprotect.SecureAbove22Preferences
import mozilla.components.lib.fetch.httpurlconnection.HttpURLConnectionClient
import mozilla.components.lib.publicsuffixlist.PublicSuffixList
import mozilla.components.service.digitalassetlinks.local.StatementApi
import mozilla.components.service.digitalassetlinks.local.StatementRelationChecker
import mozilla.components.service.location.LocationService
import mozilla.components.service.sync.logins.SyncableLoginsStorage
import mozilla.components.support.base.worker.Frequency
import org.mozilla.samples.browser.addons.AddonsActivity
import org.mozilla.samples.browser.autofill.AutofillConfirmActivity
import org.mozilla.samples.browser.autofill.AutofillSearchActivity
import org.mozilla.samples.browser.autofill.AutofillUnlockActivity
import org.mozilla.samples.browser.downloads.DownloadService
import org.mozilla.samples.browser.ext.components
import org.mozilla.samples.browser.integration.FindInPageIntegration
import org.mozilla.samples.browser.request.SampleUrlEncodedRequestInterceptor
import java.util.concurrent.TimeUnit

private const val DAY_IN_MINUTES = 24 * 60L

@SuppressLint("NewApi")
@Suppress("LargeClass")
open class DefaultComponents(private val applicationContext: Context) {
    companion object {
        const val SAMPLE_BROWSER_PREFERENCES = "sample_browser_preferences"
        const val PREF_LAUNCH_EXTERNAL_APP = "sample_browser_launch_external_app"
    }

    val preferences: SharedPreferences =
        applicationContext.getSharedPreferences(SAMPLE_BROWSER_PREFERENCES, Context.MODE_PRIVATE)

    private val securePreferences by lazy { SecureAbove22Preferences(applicationContext, "key_store") }

    val autofillConfiguration by lazy {
        AutofillConfiguration(
            storage = SyncableLoginsStorage(applicationContext, lazy { securePreferences }),
            publicSuffixList = publicSuffixList,
            unlockActivity = AutofillUnlockActivity::class.java,
            confirmActivity = AutofillConfirmActivity::class.java,
            searchActivity = AutofillSearchActivity::class.java,
            applicationName = "Sample Browser",
            httpClient = client,
        )
    }

    val publicSuffixList by lazy { PublicSuffixList(applicationContext) }

    // Engine Settings
    val engineSettings by lazy {
        DefaultSettings().apply {
            historyTrackingDelegate = HistoryDelegate(lazyHistoryStorage)
            requestInterceptor = SampleUrlEncodedRequestInterceptor(applicationContext)
            remoteDebuggingEnabled = true
            supportMultipleWindows = true
            preferredColorScheme = PreferredColorScheme.Dark
            httpsOnlyMode = Engine.HttpsOnlyMode.ENABLED
        }
    }

    val addonUpdater =
        DefaultAddonUpdater(applicationContext, Frequency(1, TimeUnit.DAYS))

    // Engine
    open val engine: Engine by lazy {
        SystemEngine(applicationContext, engineSettings)
    }

    open val client: Client by lazy { HttpURLConnectionClient() }

    val icons by lazy { BrowserIcons(applicationContext, client) }

    // Storage
    private val lazyHistoryStorage = lazy { PlacesHistoryStorage(applicationContext) }
    val historyStorage by lazy { lazyHistoryStorage.value }

    val sessionStorage by lazy { SessionStorage(applicationContext, engine) }

    val permissionStorage by lazy { OnDiskSitePermissionsStorage(applicationContext) }

    val thumbnailStorage by lazy { ThumbnailStorage(applicationContext) }

    val store by lazy {
        BrowserStore(
            middleware = listOf(
                DownloadMiddleware(applicationContext, DownloadService::class.java),
                ReaderViewMiddleware(),
                ThumbnailsMiddleware(thumbnailStorage),
                UndoMiddleware(),
                RegionMiddleware(
                    applicationContext,
                    LocationService.default(),
                ),
                SearchMiddleware(applicationContext),
                RecordingDevicesMiddleware(applicationContext),
                LastAccessMiddleware(),
                PromptMiddleware(),
                SessionPrioritizationMiddleware(),
            ) + EngineMiddleware.create(engine),
        )
    }

    val customTabsStore by lazy { CustomTabsServiceStore() }

    val sessionUseCases by lazy { SessionUseCases(store) }

    val customTabsUseCases by lazy { CustomTabsUseCases(store, sessionUseCases.loadUrl) }

    // Addons
    val addonManager by lazy {
        AddonManager(store, engine, addonCollectionProvider, addonUpdater)
    }

    val addonCollectionProvider by lazy {
        AddonCollectionProvider(
            applicationContext,
            client,
            collectionName = "7dfae8669acc4312a65e8ba5553036",
            maxCacheAgeInMinutes = DAY_IN_MINUTES,
        )
    }

    val supportedAddonsChecker by lazy {
        DefaultSupportedAddonsChecker(applicationContext, Frequency(1, TimeUnit.DAYS))
    }

    val searchUseCases by lazy {
        SearchUseCases(store, tabsUseCases, sessionUseCases)
    }

    val defaultSearchUseCase by lazy {
        { searchTerms: String ->
            searchUseCases.defaultSearch.invoke(
                searchTerms = searchTerms,
                searchEngine = null,
                parentSessionId = null,
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
        )
    }

    val webAppInterceptor by lazy {
        WebAppInterceptor(
            applicationContext,
            webAppManifestStorage,
        )
    }

    val webAppManifestStorage by lazy { ManifestStorage(applicationContext) }
    val webAppShortcutManager by lazy { WebAppShortcutManager(applicationContext, client, webAppManifestStorage) }
    val webAppUseCases by lazy { WebAppUseCases(applicationContext, store, webAppShortcutManager) }

    // Digital Asset Links checking
    val relationChecker by lazy {
        StatementRelationChecker(StatementApi(client))
    }

    // Intent
    val tabIntentProcessor by lazy {
        TabIntentProcessor(tabsUseCases, searchUseCases.newTabSearch)
    }
    val externalAppIntentProcessors by lazy {
        listOf(
            WebAppIntentProcessor(store, customTabsUseCases.addWebApp, sessionUseCases.loadUrl, webAppManifestStorage),
            CustomTabIntentProcessor(customTabsUseCases.add, applicationContext.resources),
        )
    }

    // Menu
    val menuBuilder by lazy {
        WebExtensionBrowserMenuBuilder(
            menuItems,
            store = store,
            style = WebExtensionBrowserMenuBuilder.Style(
                webExtIconTintColorResource = R.color.photonGrey90,
            ),
            onAddonsManagerTapped = {
                val intent = Intent(applicationContext, AddonsActivity::class.java)
                intent.flags = Intent.FLAG_ACTIVITY_NEW_TASK
                applicationContext.startActivity(intent)
            },
        )
    }

    private val menuItems by lazy {
        val items = mutableListOf(
            menuToolbar,
            BrowserMenuHighlightableItem(
                "No Highlight",
                R.drawable.mozac_ic_share,
                android.R.color.black,
                highlight = BrowserMenuHighlight.LowPriority(
                    notificationTint = ContextCompat.getColor(applicationContext, android.R.color.holo_green_dark),
                    label = "Highlight",
                ),
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
            SimpleBrowserMenuItem("Save to PDF") {
                sessionUseCases.saveToPdf.invoke()
            },
            SimpleBrowserMenuItem("Restore after crash") {
                sessionUseCases.crashRecovery.invoke()
            },
            BrowserMenuDivider(),
        )

        items.add(
            SimpleBrowserMenuItem("Add to homescreen") {
                MainScope().launch {
                    webAppUseCases.addToHomescreen()
                }
            }.apply {
                visible = { webAppUseCases.isPinningSupported() && store.state.selectedTabId != null }
            },
        )

        items.add(
            SimpleBrowserMenuItem("Open in App") {
                val getRedirect = appLinksUseCases.appLinkRedirect
                store.state.selectedTab?.let {
                    val redirect = getRedirect.invoke(it.content.url)
                    redirect.appIntent?.flags = Intent.FLAG_ACTIVITY_NEW_TASK
                    appLinksUseCases.openAppLink.invoke(redirect.appIntent)
                }
            }.apply {
                visible = {
                    store.state.selectedTab?.let {
                        appLinksUseCases.appLinkRedirect(it.content.url).hasExternalApp()
                    } ?: false
                }
            },
        )

        items.add(
            BrowserMenuCheckbox(
                "Request desktop site",
                {
                    store.state.selectedTab?.content?.desktopMode == true
                },
            ) { checked ->
                sessionUseCases.requestDesktopSite(checked)
            }.apply {
                visible = { store.state.selectedTab != null }
            },
        )
        items.add(
            BrowserMenuCheckbox(
                "Open links in apps",
                {
                    preferences.getBoolean(PREF_LAUNCH_EXTERNAL_APP, false)
                },
            ) { checked ->
                preferences.edit().putBoolean(PREF_LAUNCH_EXTERNAL_APP, checked).apply()
            },
        )

        items
    }

    private val menuToolbar by lazy {
        val back = BrowserMenuItemToolbar.TwoStateButton(
            primaryImageResource = mozilla.components.ui.icons.R.drawable.mozac_ic_back,
            primaryImageTintResource = R.color.photonBlue90,
            primaryContentDescription = "Back",
            isInPrimaryState = {
                store.state.selectedTab?.content?.canGoBack ?: true
            },
            disableInSecondaryState = true,
            secondaryImageTintResource = R.color.photonGrey40,
        ) {
            sessionUseCases.goBack()
        }

        val forward = BrowserMenuItemToolbar.TwoStateButton(
            primaryImageResource = mozilla.components.ui.icons.R.drawable.mozac_ic_forward,
            primaryContentDescription = "Forward",
            primaryImageTintResource = R.color.photonBlue90,
            isInPrimaryState = {
                store.state.selectedTab?.content?.canGoForward ?: true
            },
            disableInSecondaryState = true,
            secondaryImageTintResource = R.color.photonGrey40,
        ) {
            sessionUseCases.goForward()
        }

        val refresh = BrowserMenuItemToolbar.TwoStateButton(
            primaryImageResource = mozilla.components.ui.icons.R.drawable.mozac_ic_refresh,
            primaryContentDescription = "Refresh",
            primaryImageTintResource = R.color.photonBlue90,
            isInPrimaryState = {
                store.state.selectedTab?.content?.loading == false
            },
            secondaryImageResource = mozilla.components.ui.icons.R.drawable.mozac_ic_stop,
            secondaryContentDescription = "Stop",
            secondaryImageTintResource = R.color.photonBlue90,
            disableInSecondaryState = false,
        ) {
            if (store.state.selectedTab?.content?.loading == true) {
                sessionUseCases.stopLoading()
            } else {
                sessionUseCases.reload()
            }
        }

        BrowserMenuItemToolbar(listOf(back, forward, refresh))
    }

    val shippedDomainsProvider by lazy {
        ShippedDomainsProvider().also { it.initialize(applicationContext) }
    }

    val tabsUseCases: TabsUseCases by lazy { TabsUseCases(store) }
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
                        breadcrumbs: ArrayList<Breadcrumb>,
                    ): String? {
                        return null
                    }
                },
            ),
        ).install(applicationContext)
    }
}
