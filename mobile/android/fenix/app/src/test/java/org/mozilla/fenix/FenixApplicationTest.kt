/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix

import androidx.test.core.app.ApplicationProvider
import io.mockk.every
import io.mockk.mockk
import io.mockk.verify
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.webextension.DisabledFlags
import mozilla.components.concept.engine.webextension.Metadata
import mozilla.components.concept.engine.webextension.WebExtension
import mozilla.components.feature.addons.migration.DefaultSupportedAddonsChecker
import mozilla.components.service.glean.testing.GleanTestRule
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Ignore
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.GleanMetrics.Addons
import org.mozilla.fenix.GleanMetrics.Metrics
import org.mozilla.fenix.GleanMetrics.PerfStartup
import org.mozilla.fenix.GleanMetrics.Preferences
import org.mozilla.fenix.GleanMetrics.SearchDefaultEngine
import org.mozilla.fenix.components.metrics.MozillaProductDetector
import org.mozilla.fenix.components.toolbar.ToolbarPosition
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.utils.BrowsersCache
import org.mozilla.fenix.utils.Settings

@RunWith(FenixRobolectricTestRunner::class)
class FenixApplicationTest {

    @get:Rule val gleanTestRule = GleanTestRule(ApplicationProvider.getApplicationContext())

    private lateinit var application: FenixApplication
    private lateinit var browsersCache: BrowsersCache
    private lateinit var mozillaProductDetector: MozillaProductDetector
    private lateinit var browserStore: BrowserStore

    @Before
    fun setUp() {
        application = ApplicationProvider.getApplicationContext()
        browsersCache = mockk(relaxed = true)
        mozillaProductDetector = mockk(relaxed = true)
        browserStore = BrowserStore()
    }

    @Ignore("See https://github.com/mozilla-mobile/fenix/issues/18102")
    @Test
    fun `GIVEN onCreate is called THEN the duration is measured`() {
        // application.onCreate is called before the test as part of test set up:
        // https://robolectric.blogspot.com/2013/04/the-test-lifecycle-in-20.html
        assertTrue(PerfStartup.applicationOnCreate.testHasValue())
    }

    @Test
    fun `GIVEN there are unsupported addons installed WHEN subscribing for new add-ons checks THEN register for checks`() {
        val checker = mockk<DefaultSupportedAddonsChecker>(relaxed = true)
        val unSupportedExtension: WebExtension = mockk()
        val metadata: Metadata = mockk()

        every { unSupportedExtension.getMetadata() } returns metadata
        every { metadata.disabledFlags } returns DisabledFlags.select(DisabledFlags.APP_SUPPORT)

        application.subscribeForNewAddonsIfNeeded(checker, listOf(unSupportedExtension))

        verify { checker.registerForChecks() }
    }

    @Test
    fun `GIVEN there are no unsupported addons installed WHEN subscribing for new add-ons checks THEN unregister for checks`() {
        val checker = mockk<DefaultSupportedAddonsChecker>(relaxed = true)
        val unSupportedExtension: WebExtension = mockk()
        val metadata: Metadata = mockk()

        every { unSupportedExtension.getMetadata() } returns metadata
        every { metadata.disabledFlags } returns DisabledFlags.select(DisabledFlags.USER)

        application.subscribeForNewAddonsIfNeeded(checker, listOf(unSupportedExtension))

        verify { checker.unregisterForChecks() }
    }

    @Test
    fun `WHEN setStartupMetrics is called THEN sets some base metrics`() {
        val expectedAppName = "org.mozilla.fenix"
        val settings: Settings = mockk()
        every { browsersCache.all(any()).isDefaultBrowser } returns true
        every { mozillaProductDetector.getMozillaBrowserDefault(any()) } returns expectedAppName
        every { mozillaProductDetector.getInstalledMozillaProducts(any()) } returns listOf(expectedAppName)
        every { settings.adjustCampaignId } returns "ID"
        every { settings.adjustAdGroup } returns "group"
        every { settings.adjustCreative } returns "creative"
        every { settings.adjustNetwork } returns "network"
        every { settings.searchWidgetInstalled } returns true
        every { settings.openTabsCount } returns 1
        every { settings.topSitesSize } returns 2
        every { settings.installedAddonsCount } returns 3
        every { settings.installedAddonsList } returns "test1,test2,test3"
        every { settings.enabledAddonsCount } returns 2
        every { settings.enabledAddonsList } returns "test1,test2"
        every { settings.desktopBookmarksSize } returns 4
        every { settings.mobileBookmarksSize } returns 5
        every { settings.toolbarPosition } returns ToolbarPosition.BOTTOM
        every { settings.getTabViewPingString() } returns "test"
        every { settings.getTabTimeoutPingString() } returns "test"
        every { settings.shouldShowSearchSuggestions } returns true
        every { settings.shouldUseTrackingProtection } returns true
        every { settings.isRemoteDebuggingEnabled } returns true
        every { settings.isTelemetryEnabled } returns true
        every { settings.shouldShowHistorySuggestions } returns true
        every { settings.shouldShowBookmarkSuggestions } returns true
        every { settings.shouldShowClipboardSuggestions } returns true
        every { settings.shouldShowSearchShortcuts } returns true
        every { settings.openLinksInAPrivateTab } returns true
        every { settings.shouldShowSearchSuggestionsInPrivate } returns true
        every { settings.shouldShowVoiceSearch } returns true
        every { settings.openLinksInExternalApp } returns true
        every { settings.shouldUseFixedTopToolbar } returns true
        every { settings.useStandardTrackingProtection } returns true
        every { settings.switchServiceIsEnabled } returns true
        every { settings.touchExplorationIsEnabled } returns true
        every { settings.shouldUseLightTheme } returns true
        every { settings.signedInFxaAccount } returns true
        every { settings.creditCardsSavedCount } returns 1
        every { settings.creditCardsDeletedCount } returns 2
        every { settings.creditCardsAutofilledCount } returns 3

        application.setStartupMetrics(browserStore, settings, browsersCache, mozillaProductDetector)

        // Verify that browser defaults metrics are set.
        assertEquals("Mozilla", Metrics.distributionId.testGetValue())
        assertEquals(true, Metrics.defaultBrowser.testGetValue())
        assertEquals(expectedAppName, Metrics.defaultMozBrowser.testGetValue())
        assertEquals(listOf(expectedAppName), Metrics.mozillaProducts.testGetValue())
        assertEquals("ID", Metrics.adjustCampaign.testGetValue())
        assertEquals("group", Metrics.adjustAdGroup.testGetValue())
        assertEquals("creative", Metrics.adjustCreative.testGetValue())
        assertEquals("network", Metrics.adjustNetwork.testGetValue())
        assertEquals(true, Metrics.searchWidgetInstalled.testGetValue())
        assertEquals(true, Metrics.hasOpenTabs.testGetValue())
        assertEquals(1, Metrics.tabsOpenCount.testGetValue())
        assertEquals(true, Metrics.hasTopSites.testGetValue())
        assertEquals(2, Metrics.topSitesCount.testGetValue())
        assertEquals(true, Addons.hasInstalledAddons.testGetValue())
        assertEquals(1, Metrics.creditCardsSavedCount.testGetValue())
        assertEquals(2, Metrics.creditCardsDeletedCount.testGetValue())
        assertEquals(3, Metrics.creditCardsAutofillCount.testGetValue())
        assertEquals(listOf("test1", "test2", "test3"), Addons.installedAddons.testGetValue())
        assertEquals(true, Addons.hasEnabledAddons.testGetValue())
        assertEquals(listOf("test1", "test2"), Addons.enabledAddons.testGetValue())
        assertEquals(true, Preferences.searchSuggestionsEnabled.testGetValue())
        assertEquals(true, Preferences.remoteDebuggingEnabled.testGetValue())
        assertEquals(true, Preferences.telemetryEnabled.testGetValue())
        assertEquals(true, Preferences.browsingHistorySuggestion.testGetValue())
        assertEquals(true, Preferences.bookmarksSuggestion.testGetValue())
        assertEquals(true, Preferences.clipboardSuggestionsEnabled.testGetValue())
        assertEquals(true, Preferences.searchShortcutsEnabled.testGetValue())
        assertEquals(true, Preferences.openLinksInPrivate.testGetValue())
        assertEquals(true, Preferences.privateSearchSuggestions.testGetValue())
        assertEquals(true, Preferences.voiceSearchEnabled.testGetValue())
        assertEquals(true, Preferences.openLinksInAppEnabled.testGetValue())
        assertEquals(true, Preferences.signedInSync.testGetValue())
        assertEquals(emptyList<String>(), Preferences.syncItems.testGetValue())
        assertEquals("fixed_top", Preferences.toolbarPositionSetting.testGetValue())
        assertEquals("standard", Preferences.enhancedTrackingProtection.testGetValue())
        assertEquals(listOf("switch", "touch exploration"), Preferences.accessibilityServices.testGetValue())
        assertEquals("light", Preferences.userTheme.testGetValue())

        // Verify that search engine defaults are NOT set. This test does
        // not mock most of the objects telemetry is collected from.
        assertFalse(SearchDefaultEngine.code.testHasValue())
        assertFalse(SearchDefaultEngine.name.testHasValue())
        assertFalse(SearchDefaultEngine.submissionUrl.testHasValue())
    }
}
