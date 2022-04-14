/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.metrics

import android.content.Context
import mozilla.components.service.glean.Glean
import mozilla.components.service.glean.private.NoExtraKeys
import mozilla.components.support.base.log.logger.Logger
import org.mozilla.fenix.GleanMetrics.Awesomebar
import org.mozilla.fenix.GleanMetrics.BrowserSearch
import org.mozilla.fenix.GleanMetrics.HomeMenu
import org.mozilla.fenix.GleanMetrics.HomeScreen
import org.mozilla.fenix.GleanMetrics.Pings
import org.mozilla.fenix.GleanMetrics.ProgressiveWebApp
import org.mozilla.fenix.GleanMetrics.RecentSearches
import org.mozilla.fenix.GleanMetrics.RecentlyVisitedHomepage
import org.mozilla.fenix.GleanMetrics.StartOnHome
import org.mozilla.fenix.GleanMetrics.SyncedTabs
import org.mozilla.fenix.GleanMetrics.Tabs
import org.mozilla.fenix.GleanMetrics.Messaging
import org.mozilla.fenix.ext.components

private class EventWrapper<T : Enum<T>>(
    private val recorder: ((Map<T, String>?) -> Unit),
    private val keyMapper: ((String) -> T)? = null
) {

    /**
     * Converts snake_case string to camelCase.
     */
    private fun String.asCamelCase(): String {
        val parts = split("_")
        val builder = StringBuilder()

        for ((index, part) in parts.withIndex()) {
            if (index == 0) {
                builder.append(part)
            } else {
                builder.append(part[0].uppercase())
                builder.append(part.substring(1))
            }
        }

        return builder.toString()
    }

    fun track(event: Event) {
        val extras = if (keyMapper != null) {
            event.extras?.mapKeys { (key) ->
                keyMapper.invoke(key.toString().asCamelCase())
            }
        } else {
            null
        }

        @Suppress("DEPRECATION")
        // FIXME(#19967): Migrate to non-deprecated API.
        this.recorder(extras)
    }
}

@Suppress("DEPRECATION")
// FIXME(#19967): Migrate to non-deprecated API.
private val Event.wrapper: EventWrapper<*>?
    get() = when (this) {
        is Event.SearchWithAds -> EventWrapper<NoExtraKeys>(
            {
                BrowserSearch.withAds[label].add(1)
            }
        )
        is Event.SearchAdClicked -> EventWrapper<NoExtraKeys>(
            {
                BrowserSearch.adClicks[label].add(1)
            }
        )
        is Event.SearchInContent -> EventWrapper<NoExtraKeys>(
            {
                BrowserSearch.inContent[label].add(1)
            }
        )

        is Event.ProgressiveWebAppOpenFromHomescreenTap -> EventWrapper<NoExtraKeys>(
            { ProgressiveWebApp.homescreenTap.record(it) }
        )
        is Event.ProgressiveWebAppInstallAsShortcut -> EventWrapper<NoExtraKeys>(
            { ProgressiveWebApp.installTap.record(it) }
        )

        is Event.TabSettingsOpened -> EventWrapper<NoExtraKeys>(
            { Tabs.settingOpened.record(it) }
        )
        is Event.SyncedTabSuggestionClicked -> EventWrapper<NoExtraKeys>(
            { SyncedTabs.syncedTabsSuggestionClicked.record(it) }
        )

        is Event.BookmarkSuggestionClicked -> EventWrapper<NoExtraKeys>(
            { Awesomebar.bookmarkSuggestionClicked.record(it) }
        )
        is Event.ClipboardSuggestionClicked -> EventWrapper<NoExtraKeys>(
            { Awesomebar.clipboardSuggestionClicked.record(it) }
        )
        is Event.HistorySuggestionClicked -> EventWrapper<NoExtraKeys>(
            { Awesomebar.historySuggestionClicked.record(it) }
        )
        is Event.SearchActionClicked -> EventWrapper<NoExtraKeys>(
            { Awesomebar.searchActionClicked.record(it) }
        )
        is Event.SearchSuggestionClicked -> EventWrapper<NoExtraKeys>(
            { Awesomebar.searchSuggestionClicked.record(it) }
        )
        is Event.OpenedTabSuggestionClicked -> EventWrapper<NoExtraKeys>(
            { Awesomebar.openedTabSuggestionClicked.record(it) }
        )

        is Event.HomeMenuSettingsItemClicked -> EventWrapper<NoExtraKeys>(
            { HomeMenu.settingsItemClicked.record(it) }
        )

        is Event.HomeScreenDisplayed -> EventWrapper<NoExtraKeys>(
            { HomeScreen.homeScreenDisplayed.record(it) }
        )
        is Event.HomeScreenViewCount -> EventWrapper<NoExtraKeys>(
            { HomeScreen.homeScreenViewCount.add() }
        )
        is Event.HomeScreenCustomizedHomeClicked -> EventWrapper<NoExtraKeys>(
            { HomeScreen.customizeHomeClicked.record(it) }
        )
        is Event.StartOnHomeEnterHomeScreen -> EventWrapper<NoExtraKeys>(
            { StartOnHome.enterHomeScreen.record(it) }
        )

        is Event.StartOnHomeOpenTabsTray -> EventWrapper<NoExtraKeys>(
            { StartOnHome.openTabsTray.record(it) }
        )

        is Event.RecentSearchesGroupDeleted -> EventWrapper<NoExtraKeys>(
            { RecentSearches.groupDeleted.record(it) }
        )

        is Event.Messaging.MessageShown -> EventWrapper<NoExtraKeys>(
            {
                Messaging.messageShown.record(
                    Messaging.MessageShownExtra(
                        messageKey = this.messageId
                    )
                )
            }
        )
        is Event.Messaging.MessageClicked -> EventWrapper<NoExtraKeys>(
            {
                Messaging.messageClicked.record(
                    Messaging.MessageClickedExtra(
                        messageKey = this.messageId,
                        actionUuid = this.uuid
                    )
                )
            }
        )
        is Event.Messaging.MessageDismissed -> EventWrapper<NoExtraKeys>(
            {
                Messaging.messageDismissed.record(
                    Messaging.MessageDismissedExtra(
                        messageKey = this.messageId
                    )
                )
            }
        )
        is Event.Messaging.MessageMalformed -> EventWrapper<NoExtraKeys>(
            {
                Messaging.malformed.record(
                    Messaging.MalformedExtra(
                        messageKey = this.messageId
                    )
                )
            }
        )
        is Event.Messaging.MessageExpired -> EventWrapper<NoExtraKeys>(
            {
                Messaging.messageExpired.record(
                    Messaging.MessageExpiredExtra(
                        messageKey = this.messageId
                    )
                )
            }
        )

        is Event.HistoryHighlightOpened -> EventWrapper<NoExtraKeys>(
            { RecentlyVisitedHomepage.historyHighlightOpened.record() }
        )
        is Event.HistorySearchGroupOpened -> EventWrapper<NoExtraKeys>(
            { RecentlyVisitedHomepage.searchGroupOpened.record() }
        )

        // Don't record other events in Glean:
        is Event.AddBookmark -> null
        is Event.OpenedAppFirstRun -> null
        is Event.InteractWithSearchURLArea -> null
        is Event.ClearedPrivateData -> null
        is Event.DismissedOnboarding -> null
        is Event.SearchWidgetInstalled -> null
    }

/**
 * Service responsible for sending the activation and installation pings.
 */
class GleanMetricsService(
    private val context: Context
) : MetricsService {
    override val type = MetricServiceType.Data

    private val logger = Logger("GleanMetricsService")
    private var initialized = false

    private val activationPing = ActivationPing(context)
    private val installationPing = FirstSessionPing(context)

    override fun start() {
        logger.debug("Enabling Glean.")
        // Initialization of Glean already happened in FenixApplication.
        Glean.setUploadEnabled(true)

        if (initialized) return
        initialized = true

        // The code below doesn't need to execute immediately, so we'll add them to the visual
        // completeness task queue to be run later.
        context.components.performance.visualCompletenessQueue.queue.runIfReadyOrQueue {
            // We have to initialize Glean *on* the main thread, because it registers lifecycle
            // observers. However, the activation ping must be sent *off* of the main thread,
            // because it calls Google ad APIs that must be called *off* of the main thread.
            // These two things actually happen in parallel, but that should be ok because Glean
            // can handle events being recorded before it's initialized.
            Glean.registerPings(Pings)

            activationPing.checkAndSend()
            installationPing.checkAndSend()
        }
    }

    override fun stop() {
        Glean.setUploadEnabled(false)
    }

    override fun track(event: Event) {
        event.wrapper?.track(event)
    }

    override fun shouldTrack(event: Event): Boolean {
        return event.wrapper != null
    }
}
