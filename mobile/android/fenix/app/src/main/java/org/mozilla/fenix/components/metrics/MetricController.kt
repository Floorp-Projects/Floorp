/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.metrics

import androidx.annotation.VisibleForTesting
import mozilla.components.browser.menu.facts.BrowserMenuFacts
import mozilla.components.browser.toolbar.facts.ToolbarFacts
import mozilla.components.compose.browser.awesomebar.AwesomeBarFacts as ComposeAwesomeBarFacts
import mozilla.components.concept.awesomebar.AwesomeBar
import mozilla.components.feature.autofill.facts.AutofillFacts
import mozilla.components.feature.awesomebar.facts.AwesomeBarFacts
import mozilla.components.feature.awesomebar.provider.BookmarksStorageSuggestionProvider
import mozilla.components.feature.awesomebar.provider.ClipboardSuggestionProvider
import mozilla.components.feature.awesomebar.provider.HistoryStorageSuggestionProvider
import mozilla.components.feature.awesomebar.provider.SearchSuggestionProvider
import mozilla.components.feature.awesomebar.provider.SessionSuggestionProvider
import mozilla.components.feature.contextmenu.facts.ContextMenuFacts
import mozilla.components.feature.customtabs.CustomTabsFacts
import mozilla.components.feature.media.facts.MediaFacts
import mozilla.components.feature.prompts.dialog.LoginDialogFacts
import mozilla.components.feature.prompts.facts.CreditCardAutofillDialogFacts
import mozilla.components.feature.pwa.ProgressiveWebAppFacts
import mozilla.components.feature.search.telemetry.ads.AdsTelemetry
import mozilla.components.feature.search.telemetry.incontent.InContentTelemetry
import mozilla.components.feature.syncedtabs.facts.SyncedTabsFacts
import mozilla.components.feature.top.sites.facts.TopSitesFacts
import mozilla.components.service.glean.private.NoExtras
import mozilla.components.support.base.Component
import mozilla.components.support.base.facts.Action
import mozilla.components.support.base.facts.Fact
import mozilla.components.support.base.facts.FactProcessor
import mozilla.components.support.base.facts.Facts
import mozilla.components.support.base.log.logger.Logger
import mozilla.components.support.webextensions.facts.WebExtensionFacts
import org.mozilla.fenix.BuildConfig
import org.mozilla.fenix.GleanMetrics.Events
import org.mozilla.fenix.GleanMetrics.LoginDialog
import org.mozilla.fenix.GleanMetrics.MediaNotification
import org.mozilla.fenix.GleanMetrics.PerfAwesomebar
import org.mozilla.fenix.search.awesomebar.ShortcutsSuggestionProvider
import org.mozilla.fenix.utils.Settings

interface MetricController {
    fun start(type: MetricServiceType)
    fun stop(type: MetricServiceType)
    fun track(event: Event)

    companion object {
        fun create(
            services: List<MetricsService>,
            isDataTelemetryEnabled: () -> Boolean,
            isMarketingDataTelemetryEnabled: () -> Boolean,
            settings: Settings
        ): MetricController {
            return if (BuildConfig.TELEMETRY) {
                ReleaseMetricController(
                    services,
                    isDataTelemetryEnabled,
                    isMarketingDataTelemetryEnabled,
                    settings
                )
            } else DebugMetricController()
        }
    }
}

@VisibleForTesting
internal class DebugMetricController(
    private val logger: Logger = Logger()
) : MetricController {

    override fun start(type: MetricServiceType) {
        logger.debug("DebugMetricController: start")
    }

    override fun stop(type: MetricServiceType) {
        logger.debug("DebugMetricController: stop")
    }

    override fun track(event: Event) {
        logger.debug("DebugMetricController: track event: $event")
    }
}

@VisibleForTesting
@Suppress("LargeClass")
internal class ReleaseMetricController(
    private val services: List<MetricsService>,
    private val isDataTelemetryEnabled: () -> Boolean,
    private val isMarketingDataTelemetryEnabled: () -> Boolean,
    private val settings: Settings
) : MetricController {
    private var initialized = mutableSetOf<MetricServiceType>()

    init {
        Facts.registerProcessor(object : FactProcessor {
            override fun process(fact: Fact) {
                fact.process()
            }
        })
    }

    @VisibleForTesting
    internal fun Fact.process(): Unit = when (component to item) {
        Component.FEATURE_PROMPTS to LoginDialogFacts.Items.DISPLAY -> {
            LoginDialog.displayed.record(NoExtras())
        }
        Component.FEATURE_PROMPTS to LoginDialogFacts.Items.CANCEL -> {
            LoginDialog.cancelled.record(NoExtras())
        }
        Component.FEATURE_PROMPTS to LoginDialogFacts.Items.NEVER_SAVE -> {
            LoginDialog.neverSave.record(NoExtras())
        }
        Component.FEATURE_PROMPTS to LoginDialogFacts.Items.SAVE -> {
            LoginDialog.saved.record(NoExtras())
        }
        Component.FEATURE_MEDIA to MediaFacts.Items.NOTIFICATION -> {
            when (action) {
                Action.PLAY -> MediaNotification.play.record(NoExtras())
                Action.PAUSE -> MediaNotification.pause.record(NoExtras())
                else -> Unit
            }
        }
        Component.BROWSER_TOOLBAR to ToolbarFacts.Items.MENU -> {
            Events.toolbarMenuVisible.record(NoExtras())
        }
        else -> {
            this.toEvent()?.also {
                track(it)
            }
            Unit
        }
    }

    override fun start(type: MetricServiceType) {
        val isEnabled = isTelemetryEnabled(type)
        val isInitialized = isInitialized(type)
        if (!isEnabled || isInitialized) {
            return
        }

        services
            .filter { it.type == type }
            .forEach { it.start() }

        initialized.add(type)
    }

    override fun stop(type: MetricServiceType) {
        val isEnabled = isTelemetryEnabled(type)
        val isInitialized = isInitialized(type)
        if (isEnabled || !isInitialized) {
            return
        }

        services
            .filter { it.type == type }
            .forEach { it.stop() }

        initialized.remove(type)
    }

    override fun track(event: Event) {
        services
            .filter { it.shouldTrack(event) }
            .forEach {
                val isEnabled = isTelemetryEnabled(it.type)
                val isInitialized = isInitialized(it.type)
                if (!isEnabled || !isInitialized) {
                    return@forEach
                }

                it.track(event)
            }
    }

    @VisibleForTesting
    internal fun factToEvent(
        fact: Fact
    ): Event? {
        return fact.toEvent()
    }

    private fun isInitialized(type: MetricServiceType): Boolean = initialized.contains(type)

    private fun isTelemetryEnabled(type: MetricServiceType): Boolean = when (type) {
        MetricServiceType.Data -> isDataTelemetryEnabled()
        MetricServiceType.Marketing -> isMarketingDataTelemetryEnabled()
    }

    @Suppress("LongMethod", "MaxLineLength")
    private fun Fact.toEvent(): Event? = when {
        Component.FEATURE_PROMPTS == component && CreditCardAutofillDialogFacts.Items.AUTOFILL_CREDIT_CARD_FORM_DETECTED == item ->
            Event.CreditCardFormDetected
        Component.FEATURE_PROMPTS == component && CreditCardAutofillDialogFacts.Items.AUTOFILL_CREDIT_CARD_SUCCESS == item ->
            Event.CreditCardAutofilled
        Component.FEATURE_PROMPTS == component && CreditCardAutofillDialogFacts.Items.AUTOFILL_CREDIT_CARD_PROMPT_SHOWN == item ->
            Event.CreditCardAutofillPromptShown
        Component.FEATURE_PROMPTS == component && CreditCardAutofillDialogFacts.Items.AUTOFILL_CREDIT_CARD_PROMPT_EXPANDED == item ->
            Event.CreditCardAutofillPromptExpanded
        Component.FEATURE_PROMPTS == component && CreditCardAutofillDialogFacts.Items.AUTOFILL_CREDIT_CARD_PROMPT_DISMISSED == item ->
            Event.CreditCardAutofillPromptDismissed

        Component.FEATURE_CONTEXTMENU == component && ContextMenuFacts.Items.ITEM == item -> {
            metadata?.get("item")?.let { Event.ContextMenuItemTapped.create(it.toString()) }
        }
        Component.FEATURE_CONTEXTMENU == component && ContextMenuFacts.Items.TEXT_SELECTION_OPTION == item -> {
            when (metadata?.get("textSelectionOption")?.toString()) {
                CONTEXT_MENU_COPY -> Event.ContextMenuCopyTapped
                CONTEXT_MENU_SEARCH, CONTEXT_MENU_SEARCH_PRIVATELY -> Event.ContextMenuSearchTapped
                CONTEXT_MENU_SELECT_ALL -> Event.ContextMenuSelectAllTapped
                CONTEXT_MENU_SHARE -> Event.ContextMenuShareTapped
                else -> null
            }
        }

        Component.BROWSER_MENU == component && BrowserMenuFacts.Items.WEB_EXTENSION_MENU_ITEM == item -> {
            metadata?.get("id")?.let { Event.AddonsOpenInToolbarMenu(it.toString()) }
        }
        Component.FEATURE_CUSTOMTABS == component && CustomTabsFacts.Items.CLOSE == item -> Event.CustomTabsClosed
        Component.FEATURE_CUSTOMTABS == component && CustomTabsFacts.Items.ACTION_BUTTON == item -> Event.CustomTabsActionTapped

        Component.FEATURE_MEDIA == component && MediaFacts.Items.STATE == item -> {
            when (action) {
                Action.PLAY -> Event.MediaPlayState
                Action.PAUSE -> Event.MediaPauseState
                Action.STOP -> Event.MediaStopState
                else -> null
            }
        }
        Component.SUPPORT_WEBEXTENSIONS == component && WebExtensionFacts.Items.WEB_EXTENSIONS_INITIALIZED == item -> {
            metadata?.get("installed")?.let { installedAddons ->
                if (installedAddons is List<*>) {
                    settings.installedAddonsCount = installedAddons.size
                    settings.installedAddonsList = installedAddons.joinToString(",")
                }
            }

            metadata?.get("enabled")?.let { enabledAddons ->
                if (enabledAddons is List<*>) {
                    settings.enabledAddonsCount = enabledAddons.size
                    settings.enabledAddonsList = enabledAddons.joinToString(",")
                }
            }

            null
        }
        Component.COMPOSE_AWESOMEBAR == component && ComposeAwesomeBarFacts.Items.PROVIDER_DURATION == item -> {
            metadata?.get(ComposeAwesomeBarFacts.MetadataKeys.DURATION_PAIR)?.let { providerTiming ->
                require(providerTiming is Pair<*, *>) { "Expected providerTiming to be a Pair" }
                when (val provider = providerTiming.first as AwesomeBar.SuggestionProvider) {
                    is HistoryStorageSuggestionProvider -> PerfAwesomebar.historySuggestions
                    is BookmarksStorageSuggestionProvider -> PerfAwesomebar.bookmarkSuggestions
                    is SessionSuggestionProvider -> PerfAwesomebar.sessionSuggestions
                    is SearchSuggestionProvider -> PerfAwesomebar.searchEngineSuggestions
                    is ClipboardSuggestionProvider -> PerfAwesomebar.clipboardSuggestions
                    is ShortcutsSuggestionProvider -> PerfAwesomebar.shortcutsSuggestions
                    // NB: add PerfAwesomebar.syncedTabsSuggestions once we're using SyncedTabsSuggestionProvider
                    else -> {
                        Logger("Metrics").error("Unknown suggestion provider: $provider")
                        null
                    }
                }?.accumulateSamples(longArrayOf(providerTiming.second as Long))
            }
            null
        }
        Component.FEATURE_PWA == component && ProgressiveWebAppFacts.Items.HOMESCREEN_ICON_TAP == item -> {
            Event.ProgressiveWebAppOpenFromHomescreenTap
        }
        Component.FEATURE_PWA == component && ProgressiveWebAppFacts.Items.INSTALL_SHORTCUT == item -> {
            Event.ProgressiveWebAppInstallAsShortcut
        }
        Component.FEATURE_TOP_SITES == component && TopSitesFacts.Items.COUNT == item -> {
            value?.let {
                var count = 0
                try {
                    count = it.toInt()
                } catch (e: NumberFormatException) {
                    // Do nothing
                }

                settings.topSitesSize = count
            }
            null
        }
        Component.FEATURE_SYNCEDTABS == component && SyncedTabsFacts.Items.SYNCED_TABS_SUGGESTION_CLICKED == item -> {
            Event.SyncedTabSuggestionClicked
        }
        Component.FEATURE_AWESOMEBAR == component && AwesomeBarFacts.Items.BOOKMARK_SUGGESTION_CLICKED == item -> {
            Event.BookmarkSuggestionClicked
        }
        Component.FEATURE_AWESOMEBAR == component && AwesomeBarFacts.Items.CLIPBOARD_SUGGESTION_CLICKED == item -> {
            Event.ClipboardSuggestionClicked
        }
        Component.FEATURE_AWESOMEBAR == component && AwesomeBarFacts.Items.HISTORY_SUGGESTION_CLICKED == item -> {
            Event.HistorySuggestionClicked
        }
        Component.FEATURE_AWESOMEBAR == component && AwesomeBarFacts.Items.SEARCH_ACTION_CLICKED == item -> {
            Event.SearchActionClicked
        }
        Component.FEATURE_AWESOMEBAR == component && AwesomeBarFacts.Items.SEARCH_SUGGESTION_CLICKED == item -> {
            Event.SearchSuggestionClicked
        }
        Component.FEATURE_AWESOMEBAR == component && AwesomeBarFacts.Items.OPENED_TAB_SUGGESTION_CLICKED == item -> {
            Event.OpenedTabSuggestionClicked
        }
        Component.FEATURE_SEARCH == component && AdsTelemetry.SERP_ADD_CLICKED == item -> {
            Event.SearchAdClicked(value!!)
        }
        Component.FEATURE_SEARCH == component && AdsTelemetry.SERP_SHOWN_WITH_ADDS == item -> {
            Event.SearchWithAds(value!!)
        }
        Component.FEATURE_SEARCH == component && InContentTelemetry.IN_CONTENT_SEARCH == item -> {
            Event.SearchInContent(value!!)
        }
        Component.FEATURE_AUTOFILL == component && AutofillFacts.Items.AUTOFILL_REQUEST == item -> {
            val hasMatchingLogins = metadata?.get(AutofillFacts.Metadata.HAS_MATCHING_LOGINS) as Boolean?
            if (hasMatchingLogins == true) {
                Event.AndroidAutofillRequestWithLogins
            } else {
                Event.AndroidAutofillRequestWithoutLogins
            }
        }
        Component.FEATURE_AUTOFILL == component && AutofillFacts.Items.AUTOFILL_SEARCH == item -> {
            if (action == Action.SELECT) {
                Event.AndroidAutofillSearchItemSelected
            } else {
                Event.AndroidAutofillSearchDisplayed
            }
        }
        Component.FEATURE_AUTOFILL == component && AutofillFacts.Items.AUTOFILL_LOCK == item -> {
            if (action == Action.CONFIRM) {
                Event.AndroidAutofillUnlockSuccessful
            } else {
                Event.AndroidAutofillUnlockCanceled
            }
        }
        Component.FEATURE_AUTOFILL == component && AutofillFacts.Items.AUTOFILL_CONFIRMATION == item -> {
            if (action == Action.CONFIRM) {
                Event.AndroidAutofillConfirmationSuccessful
            } else {
                Event.AndroidAutofillConfirmationCanceled
            }
        }
        else -> null
    }

    companion object {
        /**
         * Text selection long press context items to be tracked.
         */
        const val CONTEXT_MENU_COPY = "org.mozilla.geckoview.COPY"
        const val CONTEXT_MENU_SEARCH = "CUSTOM_CONTEXT_MENU_SEARCH"
        const val CONTEXT_MENU_SEARCH_PRIVATELY = "CUSTOM_CONTEXT_MENU_SEARCH_PRIVATELY"
        const val CONTEXT_MENU_SELECT_ALL = "org.mozilla.geckoview.SELECT_ALL"
        const val CONTEXT_MENU_SHARE = "CUSTOM_CONTEXT_MENU_SHARE"
    }
}
