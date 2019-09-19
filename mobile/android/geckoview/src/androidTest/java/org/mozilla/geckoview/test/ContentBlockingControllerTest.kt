/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.geckoview.test

import android.support.test.filters.MediumTest
import android.support.test.runner.AndroidJUnit4
import org.hamcrest.Matchers
import org.junit.Ignore
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.geckoview.ContentBlocking
import org.mozilla.geckoview.ContentBlockingController
import org.mozilla.geckoview.GeckoSession
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule
import org.mozilla.geckoview.test.util.Callbacks

@RunWith(AndroidJUnit4::class)
@MediumTest
class ContentBlockingControllerTest : BaseSessionTest() {
    @GeckoSessionTestRule.Setting(key = GeckoSessionTestRule.Setting.Key.USE_TRACKING_PROTECTION, value = "true")
    @Test
    fun trackingProtectionException() {
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
                                Matchers.equalTo(category))
                        assertThat("URI should not be null", event.uri, Matchers.notNullValue())
                        assertThat("URI should match", event.uri, Matchers.endsWith("tracker.js"))
                    }
                })

        sessionRule.runtime.contentBlockingController.addException(sessionRule.session)

        sessionRule.runtime.contentBlockingController.checkException(sessionRule.session).accept {
            assertThat("Site should be on exceptions list", it, Matchers.equalTo(true))
        }

        var list = sessionRule.waitForResult(sessionRule.runtime.contentBlockingController.saveExceptionList())
        assertThat("Exceptions list should not be null", list, Matchers.notNullValue())
        assertThat("Exceptions list should have one entry", list.uris.size, Matchers.equalTo(1))

        sessionRule.session.reload()
        sessionRule.waitForPageStop()

        sessionRule.forCallbacksDuringWait(
                object : Callbacks.ContentBlockingDelegate {
                    @GeckoSessionTestRule.AssertCalled(false)
                    override fun onContentBlocked(session: GeckoSession,
                                                  event: ContentBlocking.BlockEvent) {
                    }
                })

        sessionRule.runtime.contentBlockingController.removeException(sessionRule.session)

        list = sessionRule.waitForResult(sessionRule.runtime.contentBlockingController.saveExceptionList())
        assertThat("Exceptions list should not be null", list, Matchers.notNullValue())
        assertThat("Exceptions list should have one entry", list.uris.size, Matchers.equalTo(0))

        sessionRule.session.reload()

        sessionRule.waitUntilCalled(
                object : Callbacks.ContentBlockingDelegate {
                    @GeckoSessionTestRule.AssertCalled(count=3)
                    override fun onContentBlocked(session: GeckoSession,
                                                  event: ContentBlocking.BlockEvent) {
                        assertThat("Category should be set",
                                event.antiTrackingCategory,
                                Matchers.equalTo(category))
                        assertThat("URI should not be null", event.uri, Matchers.notNullValue())
                        assertThat("URI should match", event.uri, Matchers.endsWith("tracker.js"))
                    }
                })
    }

    @Test
    fun importExportExceptions() {
        val category = ContentBlocking.AntiTracking.TEST
        sessionRule.runtime.settings.contentBlocking.setAntiTracking(category)
        sessionRule.session.loadTestPath(TRACKERS_PATH)

        sessionRule.waitForPageStop()

        sessionRule.runtime.contentBlockingController.addException(sessionRule.session)

        var export = sessionRule.waitForResult(sessionRule.runtime.contentBlockingController
                .saveExceptionList())
        assertThat("Exported list must not be null", export, Matchers.notNullValue())
        assertThat("Exported list must contain one entry", export.uris.size, Matchers.equalTo(1))

        val exportStr = export.toString()
        assertThat("Exported string must not be null", exportStr, Matchers.notNullValue())

        val exportJson = export.toJson()
        assertThat("Exported JSON must not be null", exportJson, Matchers.notNullValue())

        // Wipe
        sessionRule.runtime.contentBlockingController.clearExceptionList()
        export = sessionRule.waitForResult(sessionRule.runtime.contentBlockingController
                .saveExceptionList())
        assertThat("Exported list must not be null", export, Matchers.notNullValue())
        assertThat("Exported list must contain zero entries", export.uris.size, Matchers.equalTo(0))

        // Restore from String
        val importStr = sessionRule.runtime.contentBlockingController.ExceptionList(exportStr)
        sessionRule.runtime.contentBlockingController.restoreExceptionList(importStr)

        export = sessionRule.waitForResult(sessionRule.runtime.contentBlockingController
                .saveExceptionList())
        assertThat("Exported list must not be null", export, Matchers.notNullValue())
        assertThat("Exported list must contain one entry", export.uris.size, Matchers.equalTo(1))

        // Wipe
        sessionRule.runtime.contentBlockingController.clearExceptionList()
        export = sessionRule.waitForResult(sessionRule.runtime.contentBlockingController
                .saveExceptionList())
        assertThat("Exported list must not be null", export, Matchers.notNullValue())
        assertThat("Exported list must contain zero entries", export.uris.size, Matchers.equalTo(0))

        // Restore from JSON
        val importJson = sessionRule.runtime.contentBlockingController.ExceptionList(exportJson)
        sessionRule.runtime.contentBlockingController.restoreExceptionList(importJson)

        export = sessionRule.waitForResult(sessionRule.runtime.contentBlockingController
                .saveExceptionList())
        assertThat("Exported list must not be null", export, Matchers.notNullValue())
        assertThat("Exported list must contain one entry", export.uris.size, Matchers.equalTo(1))

        // Wipe so as not to break other tests.
        sessionRule.runtime.contentBlockingController.clearExceptionList()
    }
}
