/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus

import android.app.PendingIntent
import android.content.Context
import android.content.Intent
import androidx.compose.runtime.Composable
import androidx.compose.ui.platform.LocalContext
import mozilla.components.browser.icons.BrowserIcons
import mozilla.components.browser.state.engine.EngineMiddleware
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.DefaultSettings
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.fetch.Client
import mozilla.components.feature.app.links.AppLinksUseCases
import mozilla.components.feature.contextmenu.ContextMenuUseCases
import mozilla.components.feature.customtabs.store.CustomTabsServiceStore
import mozilla.components.feature.downloads.DownloadMiddleware
import mozilla.components.feature.downloads.DownloadsUseCases
import mozilla.components.feature.prompts.PromptMiddleware
import mozilla.components.feature.search.SearchUseCases
import mozilla.components.feature.search.middleware.AdsTelemetryMiddleware
import mozilla.components.feature.search.middleware.SearchMiddleware
import mozilla.components.feature.search.region.RegionMiddleware
import mozilla.components.feature.search.telemetry.ads.AdsTelemetry
import mozilla.components.feature.search.telemetry.incontent.InContentTelemetry
import mozilla.components.feature.session.SessionUseCases
import mozilla.components.feature.session.SettingsUseCases
import mozilla.components.feature.session.TrackingProtectionUseCases
import mozilla.components.feature.tabs.CustomTabsUseCases
import mozilla.components.feature.tabs.TabsUseCases
import mozilla.components.feature.top.sites.PinnedSiteStorage
import mozilla.components.feature.top.sites.TopSitesUseCases
import mozilla.components.feature.webcompat.WebCompatFeature
import mozilla.components.feature.webcompat.reporter.WebCompatReporterFeature
import mozilla.components.lib.crash.CrashReporter
import mozilla.components.lib.crash.service.CrashReporterService
import mozilla.components.lib.crash.service.GleanCrashReporterService
import mozilla.components.lib.crash.service.MozillaSocorroService
import mozilla.components.lib.crash.service.SentryService
import mozilla.components.service.location.LocationService
import mozilla.components.service.location.MozillaLocationService
import org.mozilla.focus.activity.MainActivity
import org.mozilla.focus.browser.BlockedTrackersMiddleware
import org.mozilla.focus.components.EngineProvider
import org.mozilla.focus.downloads.DownloadService
import org.mozilla.focus.engine.ClientWrapper
import org.mozilla.focus.engine.LocalizedContentInterceptor
import org.mozilla.focus.engine.SanityCheckMiddleware
import org.mozilla.focus.exceptions.ExceptionMigrationMiddleware
import org.mozilla.focus.ext.components
import org.mozilla.focus.locale.LocaleManager
import org.mozilla.focus.notification.PrivateNotificationMiddleware
import org.mozilla.focus.search.SearchFilterMiddleware
import org.mozilla.focus.search.SearchMigration
import org.mozilla.focus.state.AppState
import org.mozilla.focus.state.AppStore
import org.mozilla.focus.state.Screen
import org.mozilla.focus.telemetry.GleanMetricsService
import org.mozilla.focus.telemetry.TelemetryMiddleware
import org.mozilla.focus.topsites.DefaultTopSitesStorage
import org.mozilla.focus.utils.Settings
import java.util.Locale

/**
 * Helper object for lazily initializing components.
 */
class Components(
    context: Context,
    private val engineOverride: Engine? = null,
    private val clientOverride: Client? = null
) {
    val appStore: AppStore by lazy {
        AppStore(
            AppState(
                screen = determineInitialScreen(context),
                topSites = emptyList()
            )
        )
    }

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
        engineOverride ?: EngineProvider.createEngine(context, engineDefaultSettings).apply {
            Settings.getInstance(context).setupSafeBrowsing(this)
            WebCompatFeature.install(this)
            WebCompatReporterFeature.install(this, "focus")
        }
    }

    val client: ClientWrapper by lazy {
        ClientWrapper(clientOverride ?: EngineProvider.createClient(context))
    }

    val trackingProtectionUseCases by lazy { TrackingProtectionUseCases(store, engine) }

    val settingsUseCases by lazy { SettingsUseCases(engine, store) }

    private val locationService: LocationService by lazy {
        if (BuildConfig.MLS_TOKEN.isNullOrEmpty()) {
            LocationService.default()
        } else {
            @Suppress("DEPRECATION")
            MozillaLocationService(context, client.unwrap(), BuildConfig.MLS_TOKEN)
        }
    }

    val store by lazy {
        BrowserStore(
            middleware = listOf(
                ExceptionMigrationMiddleware(context),
                PrivateNotificationMiddleware(context),
                TelemetryMiddleware(),
                DownloadMiddleware(context, DownloadService::class.java),
                SanityCheckMiddleware(),
                // We are currently using the default location service. We should consider using
                // an actual implementation:
                // https://github.com/mozilla-mobile/focus-android/issues/4781
                RegionMiddleware(context, locationService),
                SearchMiddleware(context, migration = SearchMigration(context)),
                SearchFilterMiddleware(),
                PromptMiddleware(),
                AdsTelemetryMiddleware(adsTelemetry),
                BlockedTrackersMiddleware(context)
            ) + EngineMiddleware.create(engine)
        )
    }

    /**
     * The [CustomTabsServiceStore] holds global custom tabs related data.
     */
    val customTabsStore by lazy { CustomTabsServiceStore() }

    @Suppress("DEPRECATION")
    val sessionUseCases: SessionUseCases by lazy { SessionUseCases(store) }

    @Suppress("DEPRECATION")
    val tabsUseCases: TabsUseCases by lazy { TabsUseCases(store) }

    val searchUseCases: SearchUseCases by lazy {
        SearchUseCases(store, tabsUseCases)
    }

    val contextMenuUseCases: ContextMenuUseCases by lazy { ContextMenuUseCases(store) }

    val downloadsUseCases: DownloadsUseCases by lazy { DownloadsUseCases(store) }

    val appLinksUseCases: AppLinksUseCases by lazy { AppLinksUseCases(context.applicationContext) }

    @Suppress("DEPRECATION")
    val customTabsUseCases: CustomTabsUseCases by lazy { CustomTabsUseCases(store, sessionUseCases.loadUrl) }

    val crashReporter: CrashReporter by lazy { createCrashReporter(context) }

    val metrics: GleanMetricsService by lazy { GleanMetricsService(context) }

    val adsTelemetry: AdsTelemetry by lazy { AdsTelemetry() }

    val searchTelemetry: InContentTelemetry by lazy { InContentTelemetry() }

    val icons by lazy { BrowserIcons(context, client) }

    val topSitesStorage by lazy { DefaultTopSitesStorage(PinnedSiteStorage(context)) }

    val topSitesUseCases: TopSitesUseCases by lazy { TopSitesUseCases(topSitesStorage) }
}

private fun determineInitialScreen(context: Context): Screen {
    return if (Settings.getInstance(context).shouldShowFirstrun()) {
        Screen.FirstRun
    } else {
        Screen.Home
    }
}

private fun createCrashReporter(context: Context): CrashReporter {
    val services = mutableListOf<CrashReporterService>()

    if (!BuildConfig.SENTRY_TOKEN.isNullOrEmpty()) {
        val sentryService = SentryService(
            context,
            BuildConfig.SENTRY_TOKEN,
            tags = mapOf(
                "build_flavor" to BuildConfig.FLAVOR,
                "build_type" to BuildConfig.BUILD_TYPE,
                "locale_lang_tag" to getLocaleTag(context)
            ),
            environment = BuildConfig.BUILD_TYPE,
            sendEventForNativeCrashes = false // Do not send native crashes to Sentry
        )

        services.add(sentryService)
    }

    val socorroService = MozillaSocorroService(
        context,
        appName = "Focus",
        version = org.mozilla.geckoview.BuildConfig.MOZ_APP_VERSION,
        buildId = org.mozilla.geckoview.BuildConfig.MOZ_APP_BUILDID,
        vendor = org.mozilla.geckoview.BuildConfig.MOZ_APP_VENDOR,
        releaseChannel = org.mozilla.geckoview.BuildConfig.MOZ_UPDATE_CHANNEL
    )
    services.add(socorroService)

    val intent = Intent(context, MainActivity::class.java).apply {
        flags = Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_CLEAR_TOP
    }

    val pendingIntent = PendingIntent.getActivity(
        context,
        0,
        intent,
        0
    )

    return CrashReporter(
        context = context,
        services = services,
        telemetryServices = listOf(GleanCrashReporterService(context)),
        promptConfiguration = CrashReporter.PromptConfiguration(
            appName = context.resources.getString(R.string.app_name)
        ),
        shouldPrompt = CrashReporter.Prompt.ALWAYS,
        enabled = true,
        nonFatalCrashIntent = pendingIntent
    )
}

private fun getLocaleTag(context: Context): String {
    val currentLocale = LocaleManager.getInstance().getCurrentLocale(context)
    return if (currentLocale != null) {
        currentLocale.toLanguageTag()
    } else {
        Locale.getDefault().toLanguageTag()
    }
}

/**
 * Returns the [Components] object from within a [Composable].
 */
val components: Components
    @Composable
    get() = LocalContext.current.components
