/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// For ContentBlockingException
@file:Suppress("DEPRECATION")

package org.mozilla.geckoview.test

import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.filters.MediumTest
import org.hamcrest.Matchers.* // ktlint-disable no-wildcard-imports
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.geckoview.ContentBlocking
import org.mozilla.geckoview.ContentBlocking.AntiTracking
import org.mozilla.geckoview.ContentBlocking.CookieBannerMode
import org.mozilla.geckoview.ContentBlockingController
import org.mozilla.geckoview.GeckoSession
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.AssertCalled
@RunWith(AndroidJUnit4::class)
@MediumTest
class ContentBlockingControllerTest : BaseSessionTest() {
    // Smoke test for safe browsing settings, most testing is through platform tests
    @Test
    fun safeBrowsingSettings() {
        val contentBlocking = sessionRule.runtime.settings.contentBlocking

        val google = contentBlocking.safeBrowsingProviders.first { it.name == "google" }
        val google4 = contentBlocking.safeBrowsingProviders.first { it.name == "google4" }

        // Let's make sure the initial value of safeBrowsingProviders is correct
        assertThat(
            "Expected number of default providers",
            contentBlocking.safeBrowsingProviders.size,
            equalTo(2),
        )
        assertThat("Google legacy provider is present", google, notNullValue())
        assertThat("Google provider is present", google4, notNullValue())

        // Checks that the default provider values make sense
        assertThat(
            "Default provider values are sensible",
            google.getHashUrl,
            containsString("/safebrowsing-dummy/"),
        )
        assertThat(
            "Default provider values are sensible",
            google.advisoryUrl,
            startsWith("https://developers.google.com/"),
        )
        assertThat(
            "Default provider values are sensible",
            google4.getHashUrl,
            containsString("/safebrowsing4-dummy/"),
        )
        assertThat(
            "Default provider values are sensible",
            google4.updateUrl,
            containsString("/safebrowsing4-dummy/"),
        )
        assertThat(
            "Default provider values are sensible",
            google4.dataSharingUrl,
            startsWith("https://safebrowsing.googleapis.com/"),
        )

        // Checks that the pref value is also consistent with the runtime settings
        val originalPrefs = sessionRule.getPrefs(
            "browser.safebrowsing.provider.google4.updateURL",
            "browser.safebrowsing.provider.google4.gethashURL",
            "browser.safebrowsing.provider.google4.lists",
        )

        assertThat(
            "Initial prefs value is correct",
            originalPrefs[0] as String,
            equalTo(google4.updateUrl),
        )
        assertThat(
            "Initial prefs value is correct",
            originalPrefs[1] as String,
            equalTo(google4.getHashUrl),
        )
        assertThat(
            "Initial prefs value is correct",
            originalPrefs[2] as String,
            equalTo(google4.lists.joinToString(",")),
        )

        // Makes sure we can override a default value
        val override = ContentBlocking.SafeBrowsingProvider
            .from(ContentBlocking.GOOGLE_SAFE_BROWSING_PROVIDER)
            .updateUrl("http://test-update-url.com")
            .getHashUrl("http://test-get-hash-url.com")
            .build()

        // ... and that we can add a custom provider
        val custom = ContentBlocking.SafeBrowsingProvider
            .withName("custom-provider")
            .updateUrl("http://test-custom-update-url.com")
            .getHashUrl("http://test-custom-get-hash-url.com")
            .lists("a", "b", "c")
            .build()

        assertThat(
            "Override value is correct",
            override.updateUrl,
            equalTo("http://test-update-url.com"),
        )
        assertThat(
            "Override value is correct",
            override.getHashUrl,
            equalTo("http://test-get-hash-url.com"),
        )

        assertThat(
            "Custom provider value is correct",
            custom.updateUrl,
            equalTo("http://test-custom-update-url.com"),
        )
        assertThat(
            "Custom provider value is correct",
            custom.getHashUrl,
            equalTo("http://test-custom-get-hash-url.com"),
        )
        assertThat(
            "Custom provider value is correct",
            custom.lists,
            equalTo(arrayOf("a", "b", "c")),
        )

        contentBlocking.setSafeBrowsingProviders(override, custom)

        val prefs = sessionRule.getPrefs(
            "browser.safebrowsing.provider.google4.updateURL",
            "browser.safebrowsing.provider.google4.gethashURL",
            "browser.safebrowsing.provider.custom-provider.updateURL",
            "browser.safebrowsing.provider.custom-provider.gethashURL",
            "browser.safebrowsing.provider.custom-provider.lists",
        )

        assertThat(
            "Pref value is set correctly",
            prefs[0] as String,
            equalTo("http://test-update-url.com"),
        )
        assertThat(
            "Pref value is set correctly",
            prefs[1] as String,
            equalTo("http://test-get-hash-url.com"),
        )
        assertThat(
            "Pref value is set correctly",
            prefs[2] as String,
            equalTo("http://test-custom-update-url.com"),
        )
        assertThat(
            "Pref value is set correctly",
            prefs[3] as String,
            equalTo("http://test-custom-get-hash-url.com"),
        )
        assertThat(
            "Pref value is set correctly",
            prefs[4] as String,
            equalTo("a,b,c"),
        )

        // Restore defaults
        contentBlocking.setSafeBrowsingProviders(google, google4)

        // Checks that after restoring the providers the prefs get updated
        val restoredPrefs = sessionRule.getPrefs(
            "browser.safebrowsing.provider.google4.updateURL",
            "browser.safebrowsing.provider.google4.gethashURL",
            "browser.safebrowsing.provider.google4.lists",
        )

        assertThat(
            "Restored prefs value is correct",
            restoredPrefs[0] as String,
            equalTo(originalPrefs[0]),
        )
        assertThat(
            "Restored prefs value is correct",
            restoredPrefs[1] as String,
            equalTo(originalPrefs[1]),
        )
        assertThat(
            "Restored prefs value is correct",
            restoredPrefs[2] as String,
            equalTo(originalPrefs[2]),
        )
    }

    @Test
    fun getLog() {
        val category = ContentBlocking.AntiTracking.TEST
        sessionRule.runtime.settings.contentBlocking.setAntiTracking(category)
        mainSession.settings.useTrackingProtection = true
        mainSession.loadTestPath(TRACKERS_PATH)

        sessionRule.waitUntilCalled(object : ContentBlocking.Delegate {
            @AssertCalled(count = 1)
            override fun onContentBlocked(
                session: GeckoSession,
                event: ContentBlocking.BlockEvent,
            ) {
            }
        })

        sessionRule.waitForResult(
            sessionRule.runtime.contentBlockingController.getLog(mainSession).accept {
                assertThat("Log must not be null", it, notNullValue())
                assertThat("Log must have at least one entry", it?.size, not(0))
                it?.forEach {
                    it.blockingData.forEach {
                        assertThat(
                            "Category must match",
                            it.category,
                            equalTo(ContentBlockingController.Event.BLOCKED_TRACKING_CONTENT),
                        )
                        assertThat("Blocked must be true", it.blocked, equalTo(true))
                        assertThat("Count must be at least 1", it.count, not(0))
                    }
                }
            },
        )
    }

    @Test
    fun cookieBannerHandlingSettings() {
        // Check default value

        val contentBlocking = sessionRule.runtime.settings.contentBlocking

        assertThat(
            "Expect correct default value which is off",
            contentBlocking.cookieBannerMode,
            equalTo(CookieBannerMode.COOKIE_BANNER_MODE_DISABLED),
        )
        assertThat(
            "Expect correct default value for private browsing",
            contentBlocking.cookieBannerModePrivateBrowsing,
            equalTo(CookieBannerMode.COOKIE_BANNER_MODE_REJECT),
        )

        // Checks that the pref value is also consistent with the runtime settings
        val originalPrefs = sessionRule.getPrefs(
            "cookiebanners.service.mode",
            "cookiebanners.service.mode.privateBrowsing",
        )

        assertThat("Initial value is correct", originalPrefs[0] as Int, equalTo(contentBlocking.cookieBannerMode))
        assertThat("Initial value is correct", originalPrefs[1] as Int, equalTo(contentBlocking.cookieBannerModePrivateBrowsing))

        contentBlocking.cookieBannerMode = CookieBannerMode.COOKIE_BANNER_MODE_REJECT_OR_ACCEPT
        contentBlocking.cookieBannerModePrivateBrowsing = CookieBannerMode.COOKIE_BANNER_MODE_DISABLED

        val actualPrefs = sessionRule.getPrefs(
            "cookiebanners.service.mode",
            "cookiebanners.service.mode.privateBrowsing",
        )

        assertThat("Initial value is correct", actualPrefs[0] as Int, equalTo(contentBlocking.cookieBannerMode))
        assertThat("Initial value is correct", actualPrefs[1] as Int, equalTo(contentBlocking.cookieBannerModePrivateBrowsing))
    }

    @Test
    fun cookieBannerGlobalRulesEnabledSettings() {
        // Check default value
        val contentBlocking = sessionRule.runtime.settings.contentBlocking

        assertThat(
            "Expect correct default value which is off",
            contentBlocking.cookieBannerGlobalRulesEnabled,
            equalTo(false),
        )

        // Checks that the pref value is also consistent with the runtime settings
        val originalPrefs = sessionRule.getPrefs(
            "cookiebanners.service.enableGlobalRules",
        )

        assertThat("Actual value is correct", originalPrefs[0] as Boolean, equalTo(contentBlocking.cookieBannerGlobalRulesEnabled))

        contentBlocking.cookieBannerGlobalRulesEnabled = true

        val actualPrefs = sessionRule.getPrefs("cookiebanners.service.enableGlobalRules")

        assertThat("Actual value is correct", actualPrefs[0] as Boolean, equalTo(contentBlocking.cookieBannerGlobalRulesEnabled))
    }

    @Test
    fun cookieBannerGlobalRulesSubFramesEnabledSettings() {
        // Check default value
        val contentBlocking = sessionRule.runtime.settings.contentBlocking

        assertThat(
            "Expect correct default value which is off",
            contentBlocking.cookieBannerGlobalRulesSubFramesEnabled,
            equalTo(false),
        )

        // Checks that the pref value is also consistent with the runtime settings
        val originalPrefs = sessionRule.getPrefs(
            "cookiebanners.service.enableGlobalRules.subFrames",
        )

        assertThat("Actual value is correct", originalPrefs[0] as Boolean, equalTo(contentBlocking.cookieBannerGlobalRulesSubFramesEnabled))

        contentBlocking.cookieBannerGlobalRulesSubFramesEnabled = true

        val actualPrefs = sessionRule.getPrefs("cookiebanners.service.enableGlobalRules.subFrames")

        assertThat("Actual value is correct", actualPrefs[0] as Boolean, equalTo(contentBlocking.cookieBannerGlobalRulesSubFramesEnabled))
    }

    @Test
    fun cookieBannerHandlingDetectOnlyModeSettings() {
        // Check default value
        val contentBlocking = sessionRule.runtime.settings.contentBlocking

        assertThat(
            "Expect correct default value which is off",
            contentBlocking.cookieBannerDetectOnlyMode,
            equalTo(false),
        )

        // Checks that the pref value is also consistent with the runtime settings
        val originalPrefs = sessionRule.getPrefs(
            "cookiebanners.service.detectOnly",
        )

        assertThat(
            "Initial value is correct",
            originalPrefs[0] as Boolean,
            equalTo(contentBlocking.cookieBannerDetectOnlyMode),
        )

        contentBlocking.cookieBannerDetectOnlyMode = true

        val actualPrefs = sessionRule.getPrefs(
            "cookiebanners.service.detectOnly",
        )

        assertThat(
            "Initial value is correct",
            actualPrefs[0] as Boolean,
            equalTo(contentBlocking.cookieBannerDetectOnlyMode),
        )
    }

    @Test
    fun queryParameterStrippingSettings() {
        // Check default value
        val contentBlocking = sessionRule.runtime.settings.contentBlocking

        assertThat(
            "Expect correct default value which is off",
            contentBlocking.queryParameterStrippingEnabled,
            equalTo(false),
        )

        // Checks that the pref value is also consistent with the runtime settings
        val originalPrefs = sessionRule.getPrefs(
            "privacy.query_stripping.enabled",
        )

        assertThat(
            "Initial value is correct",
            originalPrefs[0] as Boolean,
            equalTo(contentBlocking.queryParameterStrippingEnabled),
        )

        contentBlocking.queryParameterStrippingEnabled = true

        val actualPrefs = sessionRule.getPrefs(
            "privacy.query_stripping.enabled",
        )

        assertThat(
            "The value is updated",
            actualPrefs[0] as Boolean,
            equalTo(contentBlocking.queryParameterStrippingEnabled),
        )
    }

    @Test
    fun queryParameterStrippingPrivateBrowsingSettings() {
        // Check default value
        val contentBlocking = sessionRule.runtime.settings.contentBlocking

        assertThat(
            "Expect correct default value which is off",
            contentBlocking.queryParameterStrippingPrivateBrowsingEnabled,
            equalTo(false),
        )

        // Checks that the pref value is also consistent with the runtime settings
        val originalPrefs = sessionRule.getPrefs(
            "privacy.query_stripping.enabled.pbmode",
        )

        assertThat(
            "Initial value is correct",
            originalPrefs[0] as Boolean,
            equalTo(contentBlocking.queryParameterStrippingPrivateBrowsingEnabled),
        )

        contentBlocking.queryParameterStrippingPrivateBrowsingEnabled = true

        val actualPrefs = sessionRule.getPrefs(
            "privacy.query_stripping.enabled.pbmode",
        )

        assertThat(
            "The value is updated",
            actualPrefs[0] as Boolean,
            equalTo(contentBlocking.queryParameterStrippingPrivateBrowsingEnabled),
        )
    }

    @Test
    fun queryParameterStrippingAllowListSettings() {
        // Check default value
        val contentBlocking = sessionRule.runtime.settings.contentBlocking

        assertThat(
            "Expect correct default value which is empty string",
            contentBlocking.queryParameterStrippingAllowList.joinToString(","),
            equalTo(""),
        )

        // Checks that the pref value is also consistent with the runtime settings
        val originalPrefs = sessionRule.getPrefs(
            "privacy.query_stripping.allow_list",
        )

        assertThat(
            "Initial value is correct",
            originalPrefs[0] as String,
            equalTo(contentBlocking.queryParameterStrippingAllowList.joinToString(",")),
        )

        contentBlocking.setQueryParameterStrippingAllowList("item_one", "item_two")

        val actualPrefs = sessionRule.getPrefs(
            "privacy.query_stripping.allow_list",
        )

        assertThat(
            "The value is updated",
            actualPrefs[0] as String,
            equalTo(contentBlocking.queryParameterStrippingAllowList.joinToString(",")),
        )
    }

    @Test
    fun queryParameterStrippingStripListSettings() {
        // Check default value
        val contentBlocking = sessionRule.runtime.settings.contentBlocking

        assertThat(
            "Expect correct default value which is empty string",
            contentBlocking.queryParameterStrippingStripList.joinToString(","),
            equalTo(""),
        )

        // Checks that the pref value is also consistent with the runtime settings
        val originalPrefs = sessionRule.getPrefs(
            "privacy.query_stripping.strip_list",
        )

        assertThat(
            "Initial value is correct",
            originalPrefs[0] as String,
            equalTo(contentBlocking.queryParameterStrippingStripList.joinToString(",")),
        )

        contentBlocking.setQueryParameterStrippingAllowList("item_one", "item_two")

        val actualPrefs = sessionRule.getPrefs(
            "privacy.query_stripping.strip_list",
        )

        assertThat(
            "The value is updated",
            actualPrefs[0] as String,
            equalTo(contentBlocking.queryParameterStrippingStripList.joinToString(",")),
        )
    }

    @Test
    fun toggleEmailTrackingWhenETBAddedToAntiTrackingList() {
        // check default value
        val contentBlocking = sessionRule.runtime.settings.contentBlocking

        val originalPref = sessionRule.getPrefs(
            "privacy.trackingprotection.emailtracking.enabled",
        )
        assertThat(
            "Expect correct default value which is off",
            originalPref[0] as Boolean,
            equalTo(false),
        )

        contentBlocking.setAntiTracking(AntiTracking.EMAIL)

        val updatedPref = sessionRule.getPrefs(
            "privacy.trackingprotection.emailtracking.enabled",
        )
        assertThat(
            "Expect new value which is on",
            updatedPref[0] as Boolean,
            equalTo(true),
        )
    }
}
