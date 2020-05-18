/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.geckoview.test

import androidx.test.filters.MediumTest
import androidx.test.ext.junit.runners.AndroidJUnit4
import org.hamcrest.Matchers.*
import org.junit.Ignore
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.geckoview.ContentBlocking
import org.mozilla.geckoview.ContentBlockingController
import org.mozilla.geckoview.ContentBlockingController.ContentBlockingException
import org.mozilla.geckoview.GeckoSession
import org.mozilla.geckoview.GeckoSessionSettings
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.AssertCalled
import org.mozilla.geckoview.test.util.Callbacks
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
                object : Callbacks.ContentBlockingDelegate {
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
                object : Callbacks.ContentBlockingDelegate {
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
                object : Callbacks.ContentBlockingDelegate {
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

    @GeckoSessionTestRule.Setting(key = GeckoSessionTestRule.Setting.Key.USE_TRACKING_PROTECTION, value = "true")
    @Test
    fun trackingProtectionExceptionRemoveByException() {
        // disable test on debug for frequently failing #Bug 1580223
        assumeThat(sessionRule.env.isDebugBuild, equalTo(false))
        val category = ContentBlocking.AntiTracking.TEST
        sessionRule.runtime.settings.contentBlocking.setAntiTracking(category)
        sessionRule.session.loadTestPath(TRACKERS_PATH)

        sessionRule.waitUntilCalled(
                object : Callbacks.ContentBlockingDelegate {
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
                object : Callbacks.ContentBlockingDelegate {
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
                object : Callbacks.ContentBlockingDelegate {
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
        
        sessionRule.waitUntilCalled(object : Callbacks.ContentBlockingDelegate {
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
