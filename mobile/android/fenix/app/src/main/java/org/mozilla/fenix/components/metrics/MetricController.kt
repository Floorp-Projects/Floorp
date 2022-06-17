/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.metrics

import androidx.annotation.VisibleForTesting
import mozilla.components.browser.menu.facts.BrowserMenuFacts
import mozilla.components.concept.awesomebar.AwesomeBar
import mozilla.components.feature.autofill.facts.AutofillFacts
import mozilla.components.feature.awesomebar.facts.AwesomeBarFacts
import mozilla.components.feature.awesomebar.provider.BookmarksStorageSuggestionProvider
import mozilla.components.feature.awesomebar.provider.ClipboardSuggestionProvider
import mozilla.components.feature.awesomebar.provider.HistoryStorageSuggestionProvider
import mozilla.components.feature.awesomebar.provider.SearchSuggestionProvider
import mozilla.components.feature.awesomebar.provider.SessionSuggestionProvider
import mozilla.components.feature.contextmenu.facts.ContextMenuFacts
import mozilla.components.feature.media.facts.MediaFacts
import mozilla.components.feature.prompts.dialog.LoginDialogFacts
import mozilla.components.feature.prompts.facts.CreditCardAutofillDialogFacts
import mozilla.components.feature.pwa.ProgressiveWebAppFacts
import mozilla.components.feature.search.telemetry.ads.AdsTelemetry
import mozilla.components.feature.search.telemetry.incontent.InContentTelemetry
import mozilla.components.feature.sitepermissions.SitePermissionsFacts
import mozilla.components.feature.syncedtabs.facts.SyncedTabsFacts
import mozilla.components.feature.top.sites.facts.TopSitesFacts
import mozilla.components.support.base.Component
import mozilla.components.support.base.facts.Action
import mozilla.components.support.base.facts.Fact
import mozilla.components.support.base.facts.FactProcessor
import mozilla.components.support.base.facts.Facts
import mozilla.components.support.base.log.logger.Logger
import mozilla.components.support.webextensions.facts.WebExtensionFacts
import mozilla.telemetry.glean.private.NoExtras
import org.mozilla.fenix.BuildConfig
import org.mozilla.fenix.GleanMetrics.Addons
import org.mozilla.fenix.GleanMetrics.ContextMenu
import org.mozilla.fenix.GleanMetrics.AndroidAutofill
import org.mozilla.fenix.GleanMetrics.Awesomebar
import org.mozilla.fenix.GleanMetrics.BrowserSearch
import org.mozilla.fenix.GleanMetrics.ContextualMenu
import org.mozilla.fenix.GleanMetrics.CreditCards
import org.mozilla.fenix.GleanMetrics.LoginDialog
import org.mozilla.fenix.GleanMetrics.MediaNotification
import org.mozilla.fenix.GleanMetrics.MediaState
import org.mozilla.fenix.GleanMetrics.PerfAwesomebar
import org.mozilla.fenix.GleanMetrics.ProgressiveWebApp
import org.mozilla.fenix.GleanMetrics.SitePermissions
import org.mozilla.fenix.GleanMetrics.SyncedTabs
import org.mozilla.fenix.search.awesomebar.ShortcutsSuggestionProvider
import org.mozilla.fenix.utils.Settings
import mozilla.components.compose.browser.awesomebar.AwesomeBarFacts as ComposeAwesomeBarFacts

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
    @Suppress("LongMethod")
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
        Component.FEATURE_MEDIA to MediaFacts.Items.STATE -> {
            when (action) {
                Action.PLAY -> MediaState.play.record(NoExtras())
                Action.PAUSE -> MediaState.pause.record(NoExtras())
                Action.STOP -> MediaState.stop.record(NoExtras())
                else -> Unit
            }
        }
        Component.FEATURE_MEDIA to MediaFacts.Items.NOTIFICATION -> {
            when (action) {
                Action.PLAY -> MediaNotification.play.record(NoExtras())
                Action.PAUSE -> MediaNotification.pause.record(NoExtras())
                else -> Unit
            }
        }
        Component.FEATURE_CONTEXTMENU to ContextMenuFacts.Items.ITEM -> {
            metadata?.get("item")?.let { item ->
                contextMenuAllowList[item]?.let { extraKey ->
                    ContextMenu.itemTapped.record(ContextMenu.ItemTappedExtra(extraKey))
                }
            }
            Unit
        }

        Component.BROWSER_MENU to BrowserMenuFacts.Items.WEB_EXTENSION_MENU_ITEM -> {
            metadata?.get("id")?.let {
                Addons.openAddonInToolbarMenu.record(Addons.OpenAddonInToolbarMenuExtra(it.toString()))
            }
            Unit
        }
        Component.FEATURE_PROMPTS to CreditCardAutofillDialogFacts.Items.AUTOFILL_CREDIT_CARD_FORM_DETECTED ->
            CreditCards.formDetected.record(NoExtras())
        Component.FEATURE_PROMPTS to CreditCardAutofillDialogFacts.Items.AUTOFILL_CREDIT_CARD_SUCCESS ->
            CreditCards.autofilled.record(NoExtras())
        Component.FEATURE_PROMPTS to CreditCardAutofillDialogFacts.Items.AUTOFILL_CREDIT_CARD_PROMPT_SHOWN ->
            CreditCards.autofillPromptShown.record(NoExtras())
        Component.FEATURE_PROMPTS to CreditCardAutofillDialogFacts.Items.AUTOFILL_CREDIT_CARD_PROMPT_EXPANDED ->
            CreditCards.autofillPromptExpanded.record(NoExtras())
        Component.FEATURE_PROMPTS to CreditCardAutofillDialogFacts.Items.AUTOFILL_CREDIT_CARD_PROMPT_DISMISSED ->
            CreditCards.autofillPromptDismissed.record(NoExtras())
        Component.FEATURE_PROMPTS to CreditCardAutofillDialogFacts.Items.AUTOFILL_CREDIT_CARD_CREATED ->
            CreditCards.savePromptCreate.record(NoExtras())
        Component.FEATURE_PROMPTS to CreditCardAutofillDialogFacts.Items.AUTOFILL_CREDIT_CARD_UPDATED ->
            CreditCards.savePromptUpdate.record(NoExtras())
        Component.FEATURE_AUTOFILL to AutofillFacts.Items.AUTOFILL_REQUEST -> {
            val hasMatchingLogins = metadata?.get(AutofillFacts.Metadata.HAS_MATCHING_LOGINS) as Boolean?
            if (hasMatchingLogins == true) {
                AndroidAutofill.requestMatchingLogins.record(NoExtras())
            } else {
                AndroidAutofill.requestNoMatchingLogins.record(NoExtras())
            }
        }
        Component.FEATURE_AUTOFILL to AutofillFacts.Items.AUTOFILL_SEARCH -> {
            if (action == Action.SELECT) {
                AndroidAutofill.searchItemSelected.record(NoExtras())
            } else {
                AndroidAutofill.searchDisplayed.record(NoExtras())
            }
        }
        Component.FEATURE_AUTOFILL to AutofillFacts.Items.AUTOFILL_CONFIRMATION -> {
            if (action == Action.CONFIRM) {
                AndroidAutofill.confirmSuccessful.record(NoExtras())
            } else {
                AndroidAutofill.confirmCancelled.record(NoExtras())
            }
        }
        Component.FEATURE_AUTOFILL to AutofillFacts.Items.AUTOFILL_LOCK -> {
            if (action == Action.CONFIRM) {
                AndroidAutofill.unlockSuccessful.record(NoExtras())
            } else {
                AndroidAutofill.unlockCancelled.record(NoExtras())
            }
        }
        Component.FEATURE_SYNCEDTABS to SyncedTabsFacts.Items.SYNCED_TABS_SUGGESTION_CLICKED -> {
            SyncedTabs.syncedTabsSuggestionClicked.record(NoExtras())
        }
        Component.FEATURE_AWESOMEBAR to AwesomeBarFacts.Items.BOOKMARK_SUGGESTION_CLICKED -> {
            Awesomebar.bookmarkSuggestionClicked.record(NoExtras())
        }
        Component.FEATURE_AWESOMEBAR to AwesomeBarFacts.Items.CLIPBOARD_SUGGESTION_CLICKED -> {
            Awesomebar.clipboardSuggestionClicked.record(NoExtras())
        }
        Component.FEATURE_AWESOMEBAR to AwesomeBarFacts.Items.HISTORY_SUGGESTION_CLICKED -> {
            Awesomebar.historySuggestionClicked.record(NoExtras())
        }
        Component.FEATURE_AWESOMEBAR to AwesomeBarFacts.Items.SEARCH_ACTION_CLICKED -> {
            Awesomebar.searchActionClicked.record(NoExtras())
        }
        Component.FEATURE_AWESOMEBAR to AwesomeBarFacts.Items.SEARCH_SUGGESTION_CLICKED -> {
            Awesomebar.searchSuggestionClicked.record(NoExtras())
        }
        Component.FEATURE_AWESOMEBAR to AwesomeBarFacts.Items.OPENED_TAB_SUGGESTION_CLICKED -> {
            Awesomebar.openedTabSuggestionClicked.record(NoExtras())
        }
        Component.FEATURE_CONTEXTMENU to ContextMenuFacts.Items.TEXT_SELECTION_OPTION -> {
            when (metadata?.get("textSelectionOption")?.toString()) {
                CONTEXT_MENU_COPY -> ContextualMenu.copyTapped.record(NoExtras())
                CONTEXT_MENU_SEARCH,
                CONTEXT_MENU_SEARCH_PRIVATELY -> ContextualMenu.searchTapped.record(NoExtras())
                CONTEXT_MENU_SELECT_ALL -> ContextualMenu.selectAllTapped.record(NoExtras())
                CONTEXT_MENU_SHARE -> ContextualMenu.shareTapped.record(NoExtras())
                else -> Unit
            }
        }

        Component.FEATURE_PWA to ProgressiveWebAppFacts.Items.HOMESCREEN_ICON_TAP -> {
            ProgressiveWebApp.homescreenTap.record(NoExtras())
        }
        Component.FEATURE_PWA to ProgressiveWebAppFacts.Items.INSTALL_SHORTCUT -> {
            ProgressiveWebApp.installTap.record(NoExtras())
        }

        Component.FEATURE_SEARCH to AdsTelemetry.SERP_ADD_CLICKED -> {
            BrowserSearch.adClicks[value!!].add()
        }
        Component.FEATURE_SEARCH to AdsTelemetry.SERP_SHOWN_WITH_ADDS -> {
            BrowserSearch.withAds[value!!].add()
        }
        Component.FEATURE_SEARCH to InContentTelemetry.IN_CONTENT_SEARCH -> {
            BrowserSearch.inContent[value!!].add()
        }
        Component.SUPPORT_WEBEXTENSIONS to WebExtensionFacts.Items.WEB_EXTENSIONS_INITIALIZED -> {
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
            Unit
        }
        Component.COMPOSE_AWESOMEBAR to ComposeAwesomeBarFacts.Items.PROVIDER_DURATION -> {
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
                }?.accumulateSamples(listOf(providerTiming.second as Long))
            }
            Unit
        }
        Component.FEATURE_TOP_SITES to TopSitesFacts.Items.COUNT -> {
            value?.let {
                var count = 0
                try {
                    count = it.toInt()
                } catch (e: NumberFormatException) {
                    // Do nothing
                }

                settings.topSitesSize = count
            }
            Unit
        }
        Component.FEATURE_SITEPERMISSIONS to SitePermissionsFacts.Items.PERMISSIONS -> {
            when (action) {
                Action.DISPLAY -> SitePermissions.promptShown.record(SitePermissions.PromptShownExtra(value))
                Action.CONFIRM ->
                    SitePermissions.permissionsAllowed.record(SitePermissions.PermissionsAllowedExtra(value))
                Action.CANCEL ->
                    SitePermissions.permissionsDenied.record(SitePermissions.PermissionsDeniedExtra(value))
                else -> {
                    // no-op
                }
            }
        }

        else -> {
            // no-op
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

    private fun isInitialized(type: MetricServiceType): Boolean = initialized.contains(type)

    private fun isTelemetryEnabled(type: MetricServiceType): Boolean = when (type) {
        MetricServiceType.Data -> isDataTelemetryEnabled()
        MetricServiceType.Marketing -> isMarketingDataTelemetryEnabled()
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

        /**
         * Non - Text selection long press context menu items to be tracked.
         */
        private val contextMenuAllowList = mapOf(
            "mozac.feature.contextmenu.open_in_new_tab" to "open_in_new_tab",
            "mozac.feature.contextmenu.open_in_private_tab" to "open_in_private_tab",
            "mozac.feature.contextmenu.open_image_in_new_tab" to "open_image_in_new_tab",
            "mozac.feature.contextmenu.save_image" to "save_image",
            "mozac.feature.contextmenu.share_link" to "share_link",
            "mozac.feature.contextmenu.copy_link" to "copy_link",
            "mozac.feature.contextmenu.copy_image_location" to "copy_image_location",
            "mozac.feature.contextmenu.share_image" to "share_image"
        )
    }
}
