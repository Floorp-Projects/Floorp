/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings

import androidx.core.app.NotificationManagerCompat
import androidx.fragment.app.FragmentActivity
import androidx.preference.Preference
import io.mockk.every
import io.mockk.mockk
import io.mockk.mockkObject
import io.mockk.mockkStatic
import io.mockk.unmockkObject
import io.mockk.unmockkStatic
import io.mockk.verify
import kotlinx.coroutines.test.advanceUntilIdle
import mozilla.components.browser.state.state.SearchState
import mozilla.components.browser.state.state.selectedOrDefaultSearchEngine
import mozilla.components.concept.fetch.Client
import mozilla.components.service.fxa.manager.FxaAccountManager
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.rule.MainCoroutineRule
import mozilla.components.support.test.rule.runTestOnMain
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.FeatureFlags
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.getPreferenceKey
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.utils.Settings
import org.robolectric.Robolectric
import java.io.IOException

@RunWith(FenixRobolectricTestRunner::class)
class SettingsFragmentTest {

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()
    private val settingsFragment = SettingsFragment()

    @Before
    fun setup() {
        // Mock client for fetching account avatar
        val client = mockk<Client>()
        every { client.fetch(any()) } throws IOException("test")

        every { testContext.components.core.engine.profiler } returns mockk(relaxed = true)
        every { testContext.components.core.client } returns client
        every { testContext.components.settings } returns mockk(relaxed = true)
        every { testContext.components.analytics } returns mockk(relaxed = true)
        every { testContext.components.backgroundServices } returns mockk(relaxed = true)

        mockkObject(FeatureFlags)

        val activity = Robolectric.buildActivity(FragmentActivity::class.java).create().get()
        activity.supportFragmentManager.beginTransaction()
            .add(settingsFragment, "test")
            .commitNow()
    }

    @Test
    fun `Add-on collection override pref is visible if debug menu active and feature is enabled`() = runTestOnMain {
        val settingsFragment = SettingsFragment()
        val activity = Robolectric.buildActivity(FragmentActivity::class.java).create().get()

        activity.supportFragmentManager.beginTransaction()
            .add(settingsFragment, "test")
            .commitNow()

        advanceUntilIdle()

        every { FeatureFlags.customExtensionCollectionFeature } returns true

        val preferenceAmoCollectionOverride = settingsFragment.findPreference<Preference>(
            settingsFragment.getPreferenceKey(R.string.pref_key_override_amo_collection),
        )

        settingsFragment.setupAmoCollectionOverridePreference(mockk(relaxed = true))
        assertNotNull(preferenceAmoCollectionOverride)
        assertFalse(preferenceAmoCollectionOverride!!.isVisible)

        val settings: Settings = mockk(relaxed = true)
        every { settings.showSecretDebugMenuThisSession } returns true
        settingsFragment.setupAmoCollectionOverridePreference(settings)
        assertTrue(preferenceAmoCollectionOverride.isVisible)
    }

    @Test
    fun `Add-on collection override pref is visible if already configured and feature is enabled`() = runTestOnMain {
        val settingsFragment = SettingsFragment()
        val activity = Robolectric.buildActivity(FragmentActivity::class.java).create().get()

        activity.supportFragmentManager.beginTransaction()
            .add(settingsFragment, "test")
            .commitNow()

        advanceUntilIdle()

        every { FeatureFlags.customExtensionCollectionFeature } returns true

        val preferenceAmoCollectionOverride = settingsFragment.findPreference<Preference>(
            settingsFragment.getPreferenceKey(R.string.pref_key_override_amo_collection),
        )

        settingsFragment.setupAmoCollectionOverridePreference(mockk(relaxed = true))
        assertNotNull(preferenceAmoCollectionOverride)
        assertFalse(preferenceAmoCollectionOverride!!.isVisible)

        val settings: Settings = mockk(relaxed = true)
        every { settings.showSecretDebugMenuThisSession } returns false

        every { settings.amoCollectionOverrideConfigured() } returns false
        settingsFragment.setupAmoCollectionOverridePreference(settings)
        assertFalse(preferenceAmoCollectionOverride.isVisible)

        every { settings.amoCollectionOverrideConfigured() } returns true
        settingsFragment.setupAmoCollectionOverridePreference(settings)
        assertTrue(preferenceAmoCollectionOverride.isVisible)
    }

    @Test
    fun `Add-on collection override pref is not visible if feature is disabled`() = runTestOnMain {
        val settingsFragment = SettingsFragment()
        val activity = Robolectric.buildActivity(FragmentActivity::class.java).create().get()

        activity.supportFragmentManager.beginTransaction()
            .add(settingsFragment, "test")
            .commitNow()

        advanceUntilIdle()

        every { FeatureFlags.customExtensionCollectionFeature } returns false

        val preferenceAmoCollectionOverride = settingsFragment.findPreference<Preference>(
            settingsFragment.getPreferenceKey(R.string.pref_key_override_amo_collection),
        )

        val settings: Settings = mockk(relaxed = true)
        settingsFragment.setupAmoCollectionOverridePreference(settings)
        assertNotNull(preferenceAmoCollectionOverride)
        assertFalse(preferenceAmoCollectionOverride!!.isVisible)

        every { settings.showSecretDebugMenuThisSession } returns true
        every { settings.amoCollectionOverrideConfigured() } returns true
        settingsFragment.setupAmoCollectionOverridePreference(settings)
        assertFalse(preferenceAmoCollectionOverride.isVisible)
    }

    @Test
    fun `GIVEN notifications are not allowed THEN set the appropriate summary to notification preferences`() {
        val notificationPreference = settingsFragment.requirePreference<Preference>(
            R.string.pref_key_notifications,
        )
        val summary = testContext.getString(R.string.notifications_not_allowed_summary)
        mockkStatic(NotificationManagerCompat::class)
        every { NotificationManagerCompat.from(any()).areNotificationsEnabled() } returns false
        assertTrue(notificationPreference.summary.isNullOrEmpty())

        settingsFragment.setupNotificationPreference()

        assertEquals(summary, notificationPreference.summary)
        unmockkStatic(NotificationManagerCompat::class)
    }

    @Test
    fun `GIVEN notifications are allowed THEN set the appropriate summary to notification preferences`() {
        val notificationPreference = settingsFragment.requirePreference<Preference>(
            R.string.pref_key_notifications,
        )
        val summary = testContext.getString(R.string.notifications_allowed_summary)
        mockkStatic(NotificationManagerCompat::class)
        every { NotificationManagerCompat.from(any()).areNotificationsEnabled() } returns true
        assertTrue(notificationPreference.summary.isNullOrEmpty())

        settingsFragment.setupNotificationPreference()

        assertEquals(summary, notificationPreference.summary)
        unmockkStatic(NotificationManagerCompat::class)
    }

    @Test
    fun `GIVEN the opening screen setting is set to homepage after four hours THEN set the appropriate summary to homepage preference`() {
        val homepagePreference = settingsFragment.requirePreference<Preference>(
            R.string.pref_key_home,
        )
        every { testContext.settings().alwaysOpenTheHomepageWhenOpeningTheApp } returns false
        every { testContext.settings().openHomepageAfterFourHoursOfInactivity } returns true
        every { testContext.settings().alwaysOpenTheLastTabWhenOpeningTheApp } returns false
        assertTrue(homepagePreference.summary.isNullOrEmpty())
        val summary =
            testContext.getString(R.string.opening_screen_after_four_hours_of_inactivity_summary)

        settingsFragment.setupHomepagePreference()

        assertEquals(summary, homepagePreference.summary)
    }

    @Test
    fun `GIVEN the opening screen setting is set to last tab THEN set the appropriate summary to homepage preference`() {
        val homepagePreference = settingsFragment.requirePreference<Preference>(
            R.string.pref_key_home,
        )
        every { testContext.settings().alwaysOpenTheHomepageWhenOpeningTheApp } returns false
        every { testContext.settings().openHomepageAfterFourHoursOfInactivity } returns false
        every { testContext.settings().alwaysOpenTheLastTabWhenOpeningTheApp } returns true
        assertTrue(homepagePreference.summary.isNullOrEmpty())
        val summary = testContext.getString(R.string.opening_screen_last_tab_summary)

        settingsFragment.setupHomepagePreference()

        assertEquals(summary, homepagePreference.summary)
    }

    @Test
    fun `GIVEN the opening screen setting is set to homepage THEN set the appropriate summary to homepage preference`() {
        val homepagePreference = settingsFragment.requirePreference<Preference>(
            R.string.pref_key_home,
        )
        every { testContext.settings().alwaysOpenTheHomepageWhenOpeningTheApp } returns true
        every { testContext.settings().openHomepageAfterFourHoursOfInactivity } returns false
        every { testContext.settings().alwaysOpenTheLastTabWhenOpeningTheApp } returns false
        assertTrue(homepagePreference.summary.isNullOrEmpty())
        val summary = testContext.getString(R.string.opening_screen_homepage_summary)

        settingsFragment.setupHomepagePreference()

        assertEquals(summary, homepagePreference.summary)
    }

    @Test
    fun `WHEN a custom search engine is set as default THEN it's name is set as summary for search preference`() {
        val searchEngineName = "MySearchEngine"
        val searchPreference = settingsFragment.requirePreference<Preference>(
            R.string.pref_key_search_settings,
        )
        mockkStatic("mozilla.components.browser.state.state.SearchStateKt")
        every { testContext.components.core.store.state.search } returns mockk(relaxed = true)
        every { any<SearchState>().selectedOrDefaultSearchEngine } returns mockk {
            every { name } returns searchEngineName
        }
        assertTrue(searchPreference.summary.isNullOrEmpty())

        settingsFragment.setupSearchPreference()

        assertEquals(searchEngineName, searchPreference.summary)
    }

    @Test
    fun `GIVEN the tracking protection preference is set to custom THEN set the appropriate summary`() {
        val trackingProtectionPreference = settingsFragment.requirePreference<Preference>(
            R.string.pref_key_tracking_protection_settings,
        )
        every { testContext.settings().shouldUseTrackingProtection } returns true
        every { testContext.settings().useStandardTrackingProtection } returns false
        every { testContext.settings().useStrictTrackingProtection } returns false
        every { testContext.settings().useCustomTrackingProtection } returns true
        assertTrue(trackingProtectionPreference.summary.isNullOrEmpty())
        val summary = testContext.getString(R.string.tracking_protection_custom)

        settingsFragment.setupTrackingProtectionPreference()

        assertEquals(summary, trackingProtectionPreference.summary)
    }

    @Test
    fun `GIVEN the tracking protection preference is set to strict THEN set the appropriate summary`() {
        val trackingProtectionPreference = settingsFragment.requirePreference<Preference>(
            R.string.pref_key_tracking_protection_settings,
        )
        every { testContext.settings().shouldUseTrackingProtection } returns true
        every { testContext.settings().useStandardTrackingProtection } returns false
        every { testContext.settings().useStrictTrackingProtection } returns true
        every { testContext.settings().useCustomTrackingProtection } returns false
        assertTrue(trackingProtectionPreference.summary.isNullOrEmpty())
        val summary = testContext.getString(R.string.tracking_protection_strict)

        settingsFragment.setupTrackingProtectionPreference()

        assertEquals(summary, trackingProtectionPreference.summary)
    }

    @Test
    fun `GIVEN the tracking protection preference is set to standard THEN set the appropriate summary`() {
        val trackingProtectionPreference = settingsFragment.requirePreference<Preference>(
            R.string.pref_key_tracking_protection_settings,
        )
        every { testContext.settings().shouldUseTrackingProtection } returns true
        every { testContext.settings().useStandardTrackingProtection } returns true
        every { testContext.settings().useStrictTrackingProtection } returns false
        every { testContext.settings().useCustomTrackingProtection } returns false
        assertTrue(trackingProtectionPreference.summary.isNullOrEmpty())
        val summary = testContext.getString(R.string.tracking_protection_standard)

        settingsFragment.setupTrackingProtectionPreference()

        assertEquals(summary, trackingProtectionPreference.summary)
    }

    @Test
    fun `GIVEN the tracking protection preference is disabled THEN set the appropriate summary`() {
        val trackingProtectionPreference = settingsFragment.requirePreference<Preference>(
            R.string.pref_key_tracking_protection_settings,
        )
        every { testContext.settings().shouldUseTrackingProtection } returns false
        assertTrue(trackingProtectionPreference.summary.isNullOrEmpty())
        val summary = testContext.getString(R.string.tracking_protection_off)

        settingsFragment.setupTrackingProtectionPreference()

        assertEquals(summary, trackingProtectionPreference.summary)
    }

    @Test
    fun `GIVEN the HttpsOnly is set to private tabs THEN set the appropriate preference summary`() {
        val httpsOnlyPreference = settingsFragment.findPreference<Preference>(
            settingsFragment.getPreferenceKey(R.string.pref_key_https_only_settings),
        )!!
        every { testContext.settings().shouldUseHttpsOnly } returns true
        every { testContext.settings().shouldUseHttpsOnlyInPrivateTabsOnly } returns true
        every { testContext.settings().shouldUseHttpsOnlyInAllTabs } returns false
        assertTrue(httpsOnlyPreference.summary.isNullOrEmpty())
        val summary = testContext.getString(R.string.preferences_https_only_on_private)

        settingsFragment.setupHttpsOnlyPreferences()

        assertEquals(summary, httpsOnlyPreference.summary)
    }

    @Test
    fun `GIVEN the HttpsOnly is set to all tabs THEN set the appropriate preference summary`() {
        val httpsOnlyPreference = settingsFragment.findPreference<Preference>(
            settingsFragment.getPreferenceKey(R.string.pref_key_https_only_settings),
        )!!
        every { testContext.settings().shouldUseHttpsOnly } returns true
        every { testContext.settings().shouldUseHttpsOnlyInAllTabs } returns true
        every { testContext.settings().shouldUseHttpsOnlyInPrivateTabsOnly } returns false
        assertTrue(httpsOnlyPreference.summary.isNullOrEmpty())
        val summary = testContext.getString(R.string.preferences_https_only_on_all)

        settingsFragment.setupHttpsOnlyPreferences()

        assertEquals(summary, httpsOnlyPreference.summary)
    }

    @Test
    fun `GIVEN the HttpsOnly is disabled THEN set the appropriate preference summary`() {
        val httpsOnlyPreference = settingsFragment.findPreference<Preference>(
            settingsFragment.getPreferenceKey(R.string.pref_key_https_only_settings),
        )!!
        every { testContext.settings().shouldUseHttpsOnly } returns false
        assertTrue(httpsOnlyPreference.summary.isNullOrEmpty())
        val summary = testContext.getString(R.string.preferences_https_only_off)

        settingsFragment.setupHttpsOnlyPreferences()

        assertEquals(summary, httpsOnlyPreference.summary)
    }

    @Test
    fun `GIVEN an account observer WHEN the fragment is visible THEN register it for updates`() {
        val accountManager: FxaAccountManager = mockk(relaxed = true)
        every { testContext.components.backgroundServices.accountManager } returns accountManager

        settingsFragment.onStart()

        verify { accountManager.register(settingsFragment.accountObserver, settingsFragment, true) }
    }

    @Test
    fun `GIVEN an account observer WHEN the fragment stops being visible THEN unregister it for updates`() {
        val accountManager: FxaAccountManager = mockk(relaxed = true)
        every { testContext.components.backgroundServices.accountManager } returns accountManager

        settingsFragment.onStop()

        verify { accountManager.unregister(settingsFragment.accountObserver) }
    }

    @After
    fun tearDown() {
        unmockkObject(FeatureFlags)
    }
}
