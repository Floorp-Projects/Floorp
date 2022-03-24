/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.metrics

import io.mockk.MockKAnnotations
import io.mockk.every
import io.mockk.impl.annotations.MockK
import io.mockk.mockk
import io.mockk.verify
import io.mockk.verifyAll
import mozilla.components.browser.toolbar.facts.ToolbarFacts
import mozilla.components.feature.awesomebar.facts.AwesomeBarFacts
import mozilla.components.feature.customtabs.CustomTabsFacts
import mozilla.components.feature.media.facts.MediaFacts
import mozilla.components.feature.prompts.dialog.LoginDialogFacts
import mozilla.components.feature.prompts.facts.CreditCardAutofillDialogFacts
import mozilla.components.feature.pwa.ProgressiveWebAppFacts
import mozilla.components.feature.syncedtabs.facts.SyncedTabsFacts
import mozilla.components.feature.top.sites.facts.TopSitesFacts
import mozilla.components.support.base.Component
import mozilla.components.support.base.facts.Action
import mozilla.components.support.base.facts.Fact
import mozilla.components.support.base.log.logger.Logger
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.webextensions.facts.WebExtensionFacts
import mozilla.telemetry.glean.testing.GleanTestRule
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.GleanMetrics.CustomTab
import org.mozilla.fenix.GleanMetrics.LoginDialog
import org.mozilla.fenix.GleanMetrics.MediaNotification
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.utils.Settings

@RunWith(FenixRobolectricTestRunner::class)
class MetricControllerTest {

    @get:Rule
    val gleanTestRule = GleanTestRule(testContext)

    @MockK(relaxUnitFun = true) private lateinit var dataService1: MetricsService
    @MockK(relaxUnitFun = true) private lateinit var dataService2: MetricsService
    @MockK(relaxUnitFun = true) private lateinit var marketingService1: MetricsService
    @MockK(relaxUnitFun = true) private lateinit var marketingService2: MetricsService

    @Before
    fun setup() {
        MockKAnnotations.init(this)

        every { dataService1.type } returns MetricServiceType.Data
        every { dataService2.type } returns MetricServiceType.Data
        every { marketingService1.type } returns MetricServiceType.Marketing
        every { marketingService2.type } returns MetricServiceType.Marketing
    }

    @Test
    fun `debug metric controller emits logs`() {
        val logger = mockk<Logger>(relaxed = true)
        val controller = DebugMetricController(logger)

        controller.start(MetricServiceType.Data)
        verify { logger.debug("DebugMetricController: start") }

        controller.stop(MetricServiceType.Data)
        verify { logger.debug("DebugMetricController: stop") }

        controller.track(Event.OpenedAppFirstRun)
        verify { logger.debug("DebugMetricController: track event: ${Event.OpenedAppFirstRun}") }
    }

    @Test
    fun `release metric controller starts and stops all data services`() {
        var enabled = true
        val controller = ReleaseMetricController(
            services = listOf(dataService1, marketingService1, dataService2, marketingService2),
            isDataTelemetryEnabled = { enabled },
            isMarketingDataTelemetryEnabled = { enabled },
            mockk()
        )

        controller.start(MetricServiceType.Data)
        verify { dataService1.start() }
        verify { dataService2.start() }

        enabled = false

        controller.stop(MetricServiceType.Data)
        verify { dataService1.stop() }
        verify { dataService2.stop() }

        verifyAll(inverse = true) {
            marketingService1.start()
            marketingService1.stop()
            marketingService2.start()
            marketingService2.stop()
        }
    }

    @Test
    fun `release metric controller starts data service only if enabled`() {
        val controller = ReleaseMetricController(
            services = listOf(dataService1),
            isDataTelemetryEnabled = { false },
            isMarketingDataTelemetryEnabled = { true },
            mockk()
        )

        controller.start(MetricServiceType.Data)
        verify(inverse = true) { dataService1.start() }

        controller.stop(MetricServiceType.Data)
        verify(inverse = true) { dataService1.stop() }
    }

    @Test
    fun `release metric controller starts service only once`() {
        var enabled = true
        val controller = ReleaseMetricController(
            services = listOf(dataService1),
            isDataTelemetryEnabled = { enabled },
            isMarketingDataTelemetryEnabled = { true },
            mockk()
        )

        controller.start(MetricServiceType.Data)
        controller.start(MetricServiceType.Data)
        verify(exactly = 1) { dataService1.start() }

        enabled = false

        controller.stop(MetricServiceType.Data)
        controller.stop(MetricServiceType.Data)
        verify(exactly = 1) { dataService1.stop() }
    }

    @Test
    fun `release metric controller starts and stops all marketing services`() {
        var enabled = true
        val controller = ReleaseMetricController(
            services = listOf(dataService1, marketingService1, dataService2, marketingService2),
            isDataTelemetryEnabled = { enabled },
            isMarketingDataTelemetryEnabled = { enabled },
            mockk()
        )

        controller.start(MetricServiceType.Marketing)
        verify { marketingService1.start() }
        verify { marketingService2.start() }

        enabled = false

        controller.stop(MetricServiceType.Marketing)
        verify { marketingService1.stop() }
        verify { marketingService2.stop() }

        verifyAll(inverse = true) {
            dataService1.start()
            dataService1.stop()
            dataService2.start()
            dataService2.stop()
        }
    }

    @Test
    fun `tracking events should be sent to matching service`() {
        val controller = ReleaseMetricController(
            listOf(dataService1, marketingService1),
            isDataTelemetryEnabled = { true },
            isMarketingDataTelemetryEnabled = { true },
            mockk()
        )
        every { dataService1.shouldTrack(Event.TabMediaPause) } returns false
        every { marketingService1.shouldTrack(Event.TabMediaPause) } returns true

        controller.start(MetricServiceType.Marketing)
        controller.track(Event.TabMediaPause)
        verify { marketingService1.track(Event.TabMediaPause) }
    }

    @Test
    fun `tracking events should be sent to enabled service`() {
        var enabled = true
        val controller = ReleaseMetricController(
            listOf(dataService1, marketingService1),
            isDataTelemetryEnabled = { enabled },
            isMarketingDataTelemetryEnabled = { true },
            mockk()
        )
        every { dataService1.shouldTrack(Event.TabMediaPause) } returns true
        every { marketingService1.shouldTrack(Event.TabMediaPause) } returns true

        controller.start(MetricServiceType.Marketing)
        enabled = false

        controller.track(Event.TabMediaPause)
        verify { marketingService1.track(Event.TabMediaPause) }
    }

    @Test
    fun `topsites fact should set value in SharedPreference`() {
        val enabled = true
        val settings: Settings = mockk(relaxed = true)
        val controller = ReleaseMetricController(
            services = listOf(dataService1),
            isDataTelemetryEnabled = { enabled },
            isMarketingDataTelemetryEnabled = { enabled },
            settings
        )

        val fact = Fact(
            Component.FEATURE_TOP_SITES,
            Action.INTERACTION,
            TopSitesFacts.Items.COUNT,
            "1"
        )

        verify(exactly = 0) { settings.topSitesSize = any() }
        controller.factToEvent(fact)
        verify(exactly = 1) { settings.topSitesSize = any() }
    }

    @Test
    fun `tracking synced tab event should be sent to enabled service`() {
        val controller = ReleaseMetricController(
            listOf(marketingService1),
            isDataTelemetryEnabled = { true },
            isMarketingDataTelemetryEnabled = { true },
            mockk()
        )
        every { marketingService1.shouldTrack(Event.SyncedTabSuggestionClicked) } returns true
        controller.start(MetricServiceType.Marketing)

        controller.track(Event.SyncedTabSuggestionClicked)
        verify { marketingService1.track(Event.SyncedTabSuggestionClicked) }
    }

    @Test
    fun `tracking awesomebar events should be sent to enabled service`() {
        val controller = ReleaseMetricController(
            listOf(marketingService1),
            isDataTelemetryEnabled = { true },
            isMarketingDataTelemetryEnabled = { true },
            mockk()
        )
        every { marketingService1.shouldTrack(Event.BookmarkSuggestionClicked) } returns true
        every { marketingService1.shouldTrack(Event.ClipboardSuggestionClicked) } returns true
        every { marketingService1.shouldTrack(Event.HistorySuggestionClicked) } returns true
        every { marketingService1.shouldTrack(Event.SearchActionClicked) } returns true
        every { marketingService1.shouldTrack(Event.SearchSuggestionClicked) } returns true
        every { marketingService1.shouldTrack(Event.OpenedTabSuggestionClicked) } returns true
        controller.start(MetricServiceType.Marketing)

        controller.track(Event.BookmarkSuggestionClicked)
        verify { marketingService1.track(Event.BookmarkSuggestionClicked) }

        controller.track(Event.ClipboardSuggestionClicked)
        verify { marketingService1.track(Event.ClipboardSuggestionClicked) }

        controller.track(Event.HistorySuggestionClicked)
        verify { marketingService1.track(Event.HistorySuggestionClicked) }

        controller.track(Event.SearchActionClicked)
        verify { marketingService1.track(Event.SearchActionClicked) }

        controller.track(Event.SearchSuggestionClicked)
        verify { marketingService1.track(Event.SearchSuggestionClicked) }

        controller.track(Event.OpenedTabSuggestionClicked)
        verify { marketingService1.track(Event.OpenedTabSuggestionClicked) }
    }

    @Test
    fun `web extension fact should set value in SharedPreference`() {
        val enabled = true
        val settings = Settings(testContext)
        val controller = ReleaseMetricController(
            services = listOf(dataService1),
            isDataTelemetryEnabled = { enabled },
            isMarketingDataTelemetryEnabled = { enabled },
            settings
        )
        val fact = Fact(
            Component.SUPPORT_WEBEXTENSIONS,
            Action.INTERACTION,
            WebExtensionFacts.Items.WEB_EXTENSIONS_INITIALIZED,
            metadata = mapOf(
                "installed" to listOf("test1", "test2", "test3", "test4"),
                "enabled" to listOf("test2", "test4")
            )
        )

        assertEquals(settings.installedAddonsCount, 0)
        assertEquals(settings.installedAddonsList, "")
        assertEquals(settings.enabledAddonsCount, 0)
        assertEquals(settings.enabledAddonsList, "")
        controller.factToEvent(fact)
        assertEquals(settings.installedAddonsCount, 4)
        assertEquals(settings.installedAddonsList, "test1,test2,test3,test4")
        assertEquals(settings.enabledAddonsCount, 2)
        assertEquals(settings.enabledAddonsList, "test2,test4")
    }

    @Test
    fun `credit card fact should trigger event`() {
        val enabled = true
        val settings: Settings = mockk(relaxed = true)
        val controller = ReleaseMetricController(
            services = listOf(dataService1),
            isDataTelemetryEnabled = { enabled },
            isMarketingDataTelemetryEnabled = { enabled },
            settings
        )

        var fact = Fact(
            Component.FEATURE_PROMPTS,
            Action.INTERACTION,
            CreditCardAutofillDialogFacts.Items.AUTOFILL_CREDIT_CARD_FORM_DETECTED
        )

        var event = controller.factToEvent(fact)
        assertEquals(event, Event.CreditCardFormDetected)

        fact = Fact(
            Component.FEATURE_PROMPTS,
            Action.INTERACTION,
            CreditCardAutofillDialogFacts.Items.AUTOFILL_CREDIT_CARD_SUCCESS
        )

        event = controller.factToEvent(fact)
        assertEquals(event, Event.CreditCardAutofilled)

        fact = Fact(
            Component.FEATURE_PROMPTS,
            Action.INTERACTION,
            CreditCardAutofillDialogFacts.Items.AUTOFILL_CREDIT_CARD_PROMPT_SHOWN
        )

        event = controller.factToEvent(fact)
        assertEquals(event, Event.CreditCardAutofillPromptShown)

        fact = Fact(
            Component.FEATURE_PROMPTS,
            Action.INTERACTION,
            CreditCardAutofillDialogFacts.Items.AUTOFILL_CREDIT_CARD_PROMPT_EXPANDED
        )

        event = controller.factToEvent(fact)
        assertEquals(event, Event.CreditCardAutofillPromptExpanded)

        fact = Fact(
            Component.FEATURE_PROMPTS,
            Action.INTERACTION,
            CreditCardAutofillDialogFacts.Items.AUTOFILL_CREDIT_CARD_PROMPT_DISMISSED
        )

        event = controller.factToEvent(fact)
        assertEquals(event, Event.CreditCardAutofillPromptDismissed)
    }

    @Test
    fun `credit card events should be sent to enabled service`() {
        val controller = ReleaseMetricController(
            listOf(dataService1),
            isDataTelemetryEnabled = { true },
            isMarketingDataTelemetryEnabled = { true },
            mockk()
        )
        every { dataService1.shouldTrack(Event.CreditCardSaved) } returns true
        every { dataService1.shouldTrack(Event.CreditCardDeleted) } returns true
        every { dataService1.shouldTrack(Event.CreditCardModified) } returns true
        every { dataService1.shouldTrack(Event.CreditCardFormDetected) } returns true
        every { dataService1.shouldTrack(Event.CreditCardAutofilled) } returns true
        every { dataService1.shouldTrack(Event.CreditCardAutofillPromptShown) } returns true
        every { dataService1.shouldTrack(Event.CreditCardAutofillPromptExpanded) } returns true
        every { dataService1.shouldTrack(Event.CreditCardAutofillPromptDismissed) } returns true
        every { dataService1.shouldTrack(Event.CreditCardManagementAddTapped) } returns true
        every { dataService1.shouldTrack(Event.CreditCardManagementCardTapped) } returns true

        controller.start(MetricServiceType.Data)

        controller.track(Event.CreditCardSaved)
        controller.track(Event.CreditCardDeleted)
        controller.track(Event.CreditCardModified)
        controller.track(Event.CreditCardFormDetected)
        controller.track(Event.CreditCardAutofilled)
        controller.track(Event.CreditCardAutofillPromptShown)
        controller.track(Event.CreditCardAutofillPromptExpanded)
        controller.track(Event.CreditCardAutofillPromptDismissed)
        controller.track(Event.CreditCardManagementAddTapped)
        controller.track(Event.CreditCardManagementCardTapped)

        verify { dataService1.track(Event.CreditCardSaved) }
        verify { dataService1.track(Event.CreditCardDeleted) }
        verify { dataService1.track(Event.CreditCardModified) }
        verify { dataService1.track(Event.CreditCardFormDetected) }
        verify { dataService1.track(Event.CreditCardAutofilled) }
        verify { dataService1.track(Event.CreditCardAutofillPromptShown) }
        verify { dataService1.track(Event.CreditCardAutofillPromptExpanded) }
        verify { dataService1.track(Event.CreditCardAutofillPromptDismissed) }
        verify { dataService1.track(Event.CreditCardManagementAddTapped) }
        verify { dataService1.track(Event.CreditCardManagementCardTapped) }
    }

    @Test
    fun `WHEN changing Fact(component, item) without additional vals to events THEN it returns the right event`() {
        // This naive test was added for a refactoring. It only covers the comparisons that were easy to add.
        val controller = ReleaseMetricController(emptyList(), { true }, { true }, mockk())

        val simpleMappings = listOf(
            // CreditCardAutofillDialogFacts.Items is already tested.
            Triple(Component.FEATURE_PWA, ProgressiveWebAppFacts.Items.HOMESCREEN_ICON_TAP, Event.ProgressiveWebAppOpenFromHomescreenTap),
            Triple(Component.FEATURE_PWA, ProgressiveWebAppFacts.Items.INSTALL_SHORTCUT, Event.ProgressiveWebAppInstallAsShortcut),
            Triple(Component.FEATURE_SYNCEDTABS, SyncedTabsFacts.Items.SYNCED_TABS_SUGGESTION_CLICKED, Event.SyncedTabSuggestionClicked),
            Triple(Component.FEATURE_AWESOMEBAR, AwesomeBarFacts.Items.BOOKMARK_SUGGESTION_CLICKED, Event.BookmarkSuggestionClicked),
            Triple(Component.FEATURE_AWESOMEBAR, AwesomeBarFacts.Items.CLIPBOARD_SUGGESTION_CLICKED, Event.ClipboardSuggestionClicked),
            Triple(Component.FEATURE_AWESOMEBAR, AwesomeBarFacts.Items.HISTORY_SUGGESTION_CLICKED, Event.HistorySuggestionClicked),
            Triple(Component.FEATURE_AWESOMEBAR, AwesomeBarFacts.Items.SEARCH_ACTION_CLICKED, Event.SearchActionClicked),
            Triple(Component.FEATURE_AWESOMEBAR, AwesomeBarFacts.Items.SEARCH_SUGGESTION_CLICKED, Event.SearchSuggestionClicked),
            Triple(Component.FEATURE_AWESOMEBAR, AwesomeBarFacts.Items.OPENED_TAB_SUGGESTION_CLICKED, Event.OpenedTabSuggestionClicked),
        )

        simpleMappings.forEach { (component, item, expectedEvent) ->
            val fact = Fact(component, Action.CANCEL, item)
            val message = "$expectedEvent $component $item"
            assertEquals(message, expectedEvent, controller.factToEvent(fact))
        }
    }

    @Test
    fun `WHEN processing a fact with FEATURE_PROMPTS component THEN the right metric is recorded with no extras`() {
        val controller = ReleaseMetricController(emptyList(), { true }, { true }, mockk())
        val action = mockk<Action>(relaxed = true)
        val itemsToEvents = listOf(
            LoginDialogFacts.Items.DISPLAY to LoginDialog.displayed,
            LoginDialogFacts.Items.CANCEL to LoginDialog.cancelled,
            LoginDialogFacts.Items.NEVER_SAVE to LoginDialog.neverSave,
            LoginDialogFacts.Items.SAVE to LoginDialog.saved,
        )

        itemsToEvents.forEach { (item, event) ->
            val fact = Fact(Component.FEATURE_PROMPTS, action, item)
            controller.run {
                fact.process()
            }

            assertEquals(true, event.testHasValue())
            assertEquals(1, event.testGetValue().size)
            assertEquals(null, event.testGetValue().single().extra)
        }
    }

    @Test
    fun `WHEN processing a FEATURE_MEDIA NOTIFICATION fact THEN the right metric is recorded`() {
        val controller = ReleaseMetricController(emptyList(), { true }, { true }, mockk())
        val itemsToEvents = listOf(
            Action.PLAY to MediaNotification.play,
            Action.PAUSE to MediaNotification.pause,
        )

        itemsToEvents.forEach { (action, event) ->
            val fact = Fact(Component.FEATURE_MEDIA, action, MediaFacts.Items.NOTIFICATION)
            controller.run {
                fact.process()
            }

            assertEquals(true, event.testHasValue())
            assertEquals(1, event.testGetValue().size)
            assertEquals(null, event.testGetValue().single().extra)
        }
    }

    @Test
    fun `WHEN processing a CustomTab fact THEN the right metric is recorded`() {
        val controller = ReleaseMetricController(emptyList(), { true }, { true }, mockk())
        val action = mockk<Action>(relaxed = true)
        var fact: Fact

        with(controller) {
            fact = Fact(
                Component.BROWSER_TOOLBAR,
                action,
                ToolbarFacts.Items.MENU,
                metadata = mapOf("customTab" to true)
            )
            fact.process()

            assertEquals(true, CustomTab.menu.testHasValue())
            assertEquals(1, CustomTab.menu.testGetValue().size)
            assertEquals(null, CustomTab.menu.testGetValue().single().extra)

            fact = Fact(Component.FEATURE_CUSTOMTABS, action, CustomTabsFacts.Items.ACTION_BUTTON)
            fact.process()

            assertEquals(true, CustomTab.actionButton.testHasValue())
            assertEquals(1, CustomTab.actionButton.testGetValue().size)
            assertEquals(null, CustomTab.actionButton.testGetValue().single().extra)

            fact = Fact(Component.FEATURE_CUSTOMTABS, action, CustomTabsFacts.Items.CLOSE)
            fact.process()

            assertEquals(true, CustomTab.closed.testHasValue())
            assertEquals(1, CustomTab.closed.testGetValue().size)
            assertEquals(null, CustomTab.closed.testGetValue().single().extra)
        }
    }

    @Test
    fun `search term group events should be sent to enabled service`() {
        val controller = ReleaseMetricController(
            listOf(dataService1),
            isDataTelemetryEnabled = { true },
            isMarketingDataTelemetryEnabled = { true },
            mockk()
        )
        every { dataService1.shouldTrack(Event.SearchTermGroupCount(5)) } returns true
        every { dataService1.shouldTrack(Event.AverageTabsPerSearchTermGroup(2.5)) } returns true
        every { dataService1.shouldTrack(Event.JumpBackInGroupTapped) } returns true

        controller.start(MetricServiceType.Data)

        controller.track(Event.SearchTermGroupCount(5))
        controller.track(Event.AverageTabsPerSearchTermGroup(2.5))
        controller.track(Event.JumpBackInGroupTapped)

        verify { dataService1.track(Event.SearchTermGroupCount(5)) }
        verify { dataService1.track(Event.AverageTabsPerSearchTermGroup(2.5)) }
        verify { dataService1.track(Event.JumpBackInGroupTapped) }
    }
}
