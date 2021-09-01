/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.geckoview.test

import androidx.test.filters.MediumTest
import androidx.test.ext.junit.runners.AndroidJUnit4
import org.hamcrest.Matchers.*
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.geckoview.ContentBlocking
import org.mozilla.geckoview.ContentBlockingController
import org.mozilla.geckoview.ContentBlockingController.ContentBlockingException
import org.mozilla.geckoview.GeckoSession
import org.mozilla.geckoview.GeckoSessionSettings
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.AssertCalled
import org.junit.Assume.assumeThat

@RunWith(AndroidJUnit4::class)
@MediumTest
class ContentBlockingControllerTest : BaseSessionTest() {
    private fun testTrackingProtectionException(baseSettings: GeckoSessionSettings) {
        val category = ContentBlocking.AntiTracking.TEST
        sessionRule.runtime.settings.contentBlocking.setAntiTracking(category)

        val session1 = sessionRule.createOpenSession(baseSettings)
        session1.loadTestPath(TRACKERS_PATH)

        sessionRule.waitUntilCalled(
                object : ContentBlocking.Delegate {
                    @GeckoSessionTestRule.AssertCalled(count=3)
                    override fun onContentBlocked(session: GeckoSession,
                                                  event: ContentBlocking.BlockEvent) {
                        assertThat("Category should be set",
                                event.antiTrackingCategory,
                                equalTo(category))
                        assertThat("URI should not be null", event.uri, notNullValue())
                        assertThat("URI should match", event.uri, endsWith("tracker.js"))
                    }
                })

        // Add exception for this site.
        sessionRule.runtime.contentBlockingController.addException(session1)

        sessionRule.runtime.contentBlockingController.checkException(session1).accept {
            assertThat("Site should be on exceptions list", it, equalTo(true))
        }

        var list = sessionRule.waitForResult(sessionRule.runtime.contentBlockingController.saveExceptionList())
        assertThat("Exceptions list should not be null", list, notNullValue())

        if (baseSettings.usePrivateMode) {
            assertThat(
                    "Exceptions list should be empty",
                    list.size,
                    equalTo(0))
        } else {
            assertThat(
                    "Exceptions list should have one entry",
                    list.size,
                    equalTo(1))
        }

        session1.reload()
        sessionRule.waitForPageStop()

        sessionRule.forCallbacksDuringWait(
                object : ContentBlocking.Delegate {
                    @GeckoSessionTestRule.AssertCalled(false)
                    override fun onContentBlocked(session: GeckoSession,
                                                  event: ContentBlocking.BlockEvent) {
                    }
                })

        // Remove exception for this site by passing GeckoSession.
        sessionRule.runtime.contentBlockingController.removeException(session1)

        list = sessionRule.waitForResult(
                sessionRule.runtime.contentBlockingController.saveExceptionList())
        assertThat("Exceptions list should not be null", list, notNullValue())
        assertThat("Exceptions list should be empty", list.size, equalTo(0))

        session1.reload()

        sessionRule.waitUntilCalled(
                object : ContentBlocking.Delegate {
                    @GeckoSessionTestRule.AssertCalled(count=3)
                    override fun onContentBlocked(session: GeckoSession,
                                                  event: ContentBlocking.BlockEvent) {
                        assertThat("Category should be set",
                                event.antiTrackingCategory,
                                equalTo(category))
                        assertThat("URI should not be null", event.uri, notNullValue())
                        assertThat("URI should match", event.uri, endsWith("tracker.js"))
                    }
                })
    }

    @GeckoSessionTestRule.Setting(key = GeckoSessionTestRule.Setting.Key.USE_TRACKING_PROTECTION, value = "true")
    @Test
    fun trackingProtectionExceptionPrivateMode() {
        // disable test on debug for frequently failing #Bug 1580223
        assumeThat(sessionRule.env.isDebugBuild, equalTo(false))

        testTrackingProtectionException(
                GeckoSessionSettings.Builder(mainSession.settings)
                .usePrivateMode(true)
                .build())
    }

    @GeckoSessionTestRule.Setting(key = GeckoSessionTestRule.Setting.Key.USE_TRACKING_PROTECTION, value = "true")
    @Test
    fun trackingProtectionException() {
        // disable test on debug for frequently failing #Bug 1580223
        assumeThat(sessionRule.env.isDebugBuild, equalTo(false))

        testTrackingProtectionException(mainSession.settings)
    }

    @Test
    // Smoke test for safe browsing settings, most testing is through platform tests
    fun safeBrowsingSettings() {
        val contentBlocking = sessionRule.runtime.settings.contentBlocking

        val google = contentBlocking.safeBrowsingProviders.first { it.name == "google" }
        val google4 = contentBlocking.safeBrowsingProviders.first { it.name == "google4" }

        // Let's make sure the initial value of safeBrowsingProviders is correct
        assertThat("Expected number of default providers",
                contentBlocking.safeBrowsingProviders.size,
                equalTo(2))
        assertThat("Google legacy provider is present", google, notNullValue())
        assertThat("Google provider is present", google4, notNullValue())

        // Checks that the default provider values make sense
        assertThat("Default provider values are sensible",
            google.getHashUrl, containsString("/safebrowsing-dummy/"))
        assertThat("Default provider values are sensible",
                google.advisoryUrl, startsWith("https://developers.google.com/"))
        assertThat("Default provider values are sensible",
                google4.getHashUrl, containsString("/safebrowsing4-dummy/"))
        assertThat("Default provider values are sensible",
                google4.updateUrl, containsString("/safebrowsing4-dummy/"))
        assertThat("Default provider values are sensible",
                google4.dataSharingUrl, startsWith("https://safebrowsing.googleapis.com/"))

        // Checks that the pref value is also consistent with the runtime settings
        val originalPrefs = sessionRule.getPrefs(
                "browser.safebrowsing.provider.google4.updateURL",
                "browser.safebrowsing.provider.google4.gethashURL",
                "browser.safebrowsing.provider.google4.lists"
        )

        assertThat("Initial prefs value is correct",
                originalPrefs[0] as String, equalTo(google4.updateUrl))
        assertThat("Initial prefs value is correct",
                originalPrefs[1] as String, equalTo(google4.getHashUrl))
        assertThat("Initial prefs value is correct",
                originalPrefs[2] as String, equalTo(google4.lists.joinToString(",")))

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

        assertThat("Override value is correct",
                override.updateUrl, equalTo("http://test-update-url.com"))
        assertThat("Override value is correct",
                override.getHashUrl, equalTo("http://test-get-hash-url.com"))

        assertThat("Custom provider value is correct",
                custom.updateUrl, equalTo("http://test-custom-update-url.com"))
        assertThat("Custom provider value is correct",
                custom.getHashUrl, equalTo("http://test-custom-get-hash-url.com"))
        assertThat("Custom provider value is correct",
                custom.lists, equalTo(arrayOf("a", "b", "c")))

        contentBlocking.setSafeBrowsingProviders(override, custom)

        val prefs = sessionRule.getPrefs(
                "browser.safebrowsing.provider.google4.updateURL",
                "browser.safebrowsing.provider.google4.gethashURL",
                "browser.safebrowsing.provider.custom-provider.updateURL",
                "browser.safebrowsing.provider.custom-provider.gethashURL",
                "browser.safebrowsing.provider.custom-provider.lists")

        assertThat("Pref value is set correctly",
                prefs[0] as String, equalTo("http://test-update-url.com"))
        assertThat("Pref value is set correctly",
                prefs[1] as String, equalTo("http://test-get-hash-url.com"))
        assertThat("Pref value is set correctly",
                prefs[2] as String, equalTo("http://test-custom-update-url.com"))
        assertThat("Pref value is set correctly",
                prefs[3] as String, equalTo("http://test-custom-get-hash-url.com"))
        assertThat("Pref value is set correctly",
                prefs[4] as String, equalTo("a,b,c"))

        // Restore defaults
        contentBlocking.setSafeBrowsingProviders(google, google4)

        // Checks that after restoring the providers the prefs get updated
        val restoredPrefs = sessionRule.getPrefs(
                "browser.safebrowsing.provider.google4.updateURL",
                "browser.safebrowsing.provider.google4.gethashURL",
                "browser.safebrowsing.provider.google4.lists")

        assertThat("Restored prefs value is correct",
                restoredPrefs[0] as String, equalTo(originalPrefs[0]))
        assertThat("Restored prefs value is correct",
                restoredPrefs[1] as String, equalTo(originalPrefs[1]))
        assertThat("Restored prefs value is correct",
                restoredPrefs[2] as String, equalTo(originalPrefs[2]))
    }

    @GeckoSessionTestRule.Setting(key = GeckoSessionTestRule.Setting.Key.USE_TRACKING_PROTECTION, value = "true")
    @Test
    fun trackingProtectionExceptionRemoveByException() {
        // disable test on debug for frequently failing #Bug 1580223
        assumeThat(sessionRule.env.isDebugBuild, equalTo(false))
        val category = ContentBlocking.AntiTracking.TEST
        sessionRule.runtime.settings.contentBlocking.setAntiTracking(category)
        sessionRule.session.loadTestPath(TRACKERS_PATH)

        sessionRule.waitUntilCalled(
                object : ContentBlocking.Delegate {
                    @GeckoSessionTestRule.AssertCalled(count=3)
                    override fun onContentBlocked(session: GeckoSession,
                                                  event: ContentBlocking.BlockEvent) {
                        assertThat("Category should be set",
                                event.antiTrackingCategory,
                                equalTo(category))
                        assertThat("URI should not be null", event.uri, notNullValue())
                        assertThat("URI should match", event.uri, endsWith("tracker.js"))
                    }
                })

        // Add exception for this site.
        sessionRule.runtime.contentBlockingController.addException(sessionRule.session)

        sessionRule.runtime.contentBlockingController.checkException(sessionRule.session).accept {
            assertThat("Site should be on exceptions list", it, equalTo(true))
        }

        var list = sessionRule.waitForResult(sessionRule.runtime.contentBlockingController.saveExceptionList())
        assertThat("Exceptions list should not be null", list, notNullValue())
        assertThat("Exceptions list should have one entry", list.size, equalTo(1))

        sessionRule.session.reload()
        sessionRule.waitForPageStop()

        sessionRule.forCallbacksDuringWait(
                object : ContentBlocking.Delegate {
                    @GeckoSessionTestRule.AssertCalled(false)
                    override fun onContentBlocked(session: GeckoSession,
                                                  event: ContentBlocking.BlockEvent) {
                    }
                })

        // Remove exception for this site by passing ContentBlockingException.
        sessionRule.runtime.contentBlockingController.removeException(list.get(0))

        list = sessionRule.waitForResult(sessionRule.runtime.contentBlockingController.saveExceptionList())
        assertThat("Exceptions list should not be null", list, notNullValue())
        assertThat("Exceptions list should have one entry", list.size, equalTo(0))

        sessionRule.session.reload()

        sessionRule.waitUntilCalled(
                object : ContentBlocking.Delegate {
                    @GeckoSessionTestRule.AssertCalled(count=3)
                    override fun onContentBlocked(session: GeckoSession,
                                                  event: ContentBlocking.BlockEvent) {
                        assertThat("Category should be set",
                                event.antiTrackingCategory,
                                equalTo(category))
                        assertThat("URI should not be null", event.uri, notNullValue())
                        assertThat("URI should match", event.uri, endsWith("tracker.js"))
                    }
                })
    }

    @Test
    fun importExportExceptions() {
        // May provide useful info for 1580375.
        sessionRule.setPrefsUntilTestEnd(mapOf("browser.safebrowsing.debug" to true))

        val category = ContentBlocking.AntiTracking.TEST
        sessionRule.runtime.settings.contentBlocking.setAntiTracking(category)
        sessionRule.session.loadTestPath(TRACKERS_PATH)

        sessionRule.waitForPageStop()

        sessionRule.runtime.contentBlockingController.addException(sessionRule.session)

        var export = sessionRule.waitForResult(sessionRule.runtime.contentBlockingController
                .saveExceptionList())
        assertThat("Exported list must not be null", export, notNullValue())
        assertThat("Exported list must contain one entry", export.size, equalTo(1))

        val exportJson = export.get(0).toJson()
        assertThat("Exported JSON must not be null", exportJson, notNullValue())

        // Wipe
        sessionRule.runtime.contentBlockingController.clearExceptionList()
        export = sessionRule.waitForResult(sessionRule.runtime.contentBlockingController
                .saveExceptionList())
        assertThat("Exported list must not be null", export, notNullValue())
        assertThat("Exported list must contain zero entries", export.size, equalTo(0))

        // Restore from JSON
        val importJson = listOf(ContentBlockingException.fromJson(exportJson))
        sessionRule.runtime.contentBlockingController.restoreExceptionList(importJson)

        export = sessionRule.waitForResult(sessionRule.runtime.contentBlockingController
                .saveExceptionList())
        assertThat("Exported list must not be null", export, notNullValue())
        assertThat("Exported list must contain one entry", export.size, equalTo(1))

        // Wipe so as not to break other tests.
        sessionRule.runtime.contentBlockingController.clearExceptionList()
    }

    @Test
    fun getLog() {
        val category = ContentBlocking.AntiTracking.TEST
        sessionRule.runtime.settings.contentBlocking.setAntiTracking(category)
        sessionRule.session.settings.useTrackingProtection = true
        sessionRule.session.loadTestPath(TRACKERS_PATH)
        
        sessionRule.waitUntilCalled(object : ContentBlocking.Delegate {
            @AssertCalled(count = 1)
            override fun onContentBlocked(session: GeckoSession,
                                          event: ContentBlocking.BlockEvent) {

            }
        })

        sessionRule.waitForResult(sessionRule.runtime.contentBlockingController.getLog(sessionRule.session).accept {
            assertThat("Log must not be null", it, notNullValue())
            assertThat("Log must have at least one entry", it?.size, not(0))
            it?.forEach {
                it.blockingData.forEach {
                    assertThat("Category must match", it.category,
                            equalTo(ContentBlockingController.Event.BLOCKED_TRACKING_CONTENT))
                    assertThat("Blocked must be true", it.blocked, equalTo(true))
                    assertThat("Count must be at least 1", it.count, not(0))
                }
            }
        })
    }
}
