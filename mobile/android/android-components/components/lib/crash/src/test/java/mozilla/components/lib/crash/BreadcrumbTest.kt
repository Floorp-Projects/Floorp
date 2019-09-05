/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.crash

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.lib.crash.service.CrashReporterService
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.spy

@RunWith(AndroidJUnit4::class)
class BreadcrumbTest {

    @Before
    fun setUp() {
        CrashReporter.reset()
    }

    @Test
    fun `RecordBreadCrumb stores breadCrumb in reporter`() {
        val testMessage = "test_Message"
        val testData = hashMapOf("1" to "one", "2" to "two")
        val testCategory = "testing_category"
        val testLevel = Breadcrumb.Level.CRITICAL
        val testType = Breadcrumb.Type.USER

        val service = object : CrashReporterService {
            override fun report(crash: Crash.UncaughtExceptionCrash) {
            }

            override fun report(crash: Crash.NativeCodeCrash) {
            }
        }

        val reporter = spy(CrashReporter(
                services = listOf(service),
                shouldPrompt = CrashReporter.Prompt.NEVER
        ).install(testContext))

        reporter.recordCrashBreadcrumb(
                Breadcrumb(testMessage, testData, testCategory, testLevel, testType)
        )

        assertEquals(reporter.crashBreadcrumbs[0].message, testMessage)
        assertEquals(reporter.crashBreadcrumbs[0].data, testData)
        assertEquals(reporter.crashBreadcrumbs[0].category, testCategory)
        assertEquals(reporter.crashBreadcrumbs[0].level, testLevel)
        assertEquals(reporter.crashBreadcrumbs[0].type, testType)
    }

    @Test
    fun `Reporter stores current number of breadcrumbs`() {
        val testMessage = "test_Message"
        val testData = hashMapOf("1" to "one", "2" to "two")
        val testCategory = "testing_category"
        val testLevel = Breadcrumb.Level.CRITICAL
        val testType = Breadcrumb.Type.USER

        val service = object : CrashReporterService {
            override fun report(crash: Crash.UncaughtExceptionCrash) {
            }

            override fun report(crash: Crash.NativeCodeCrash) {
            }
        }

        val reporter = spy(CrashReporter(
                services = listOf(service),
                shouldPrompt = CrashReporter.Prompt.NEVER
        ).install(testContext))

        reporter.recordCrashBreadcrumb(
                Breadcrumb(testMessage, testData, testCategory, testLevel, testType)
        )
        assertEquals(reporter.crashBreadcrumbs.size, 1)

        reporter.recordCrashBreadcrumb(
                Breadcrumb(testMessage, testData, testCategory, testLevel, testType)
        )
        assertEquals(reporter.crashBreadcrumbs.size, 2)

        reporter.recordCrashBreadcrumb(
                Breadcrumb(testMessage, testData, testCategory, testLevel, testType)
        )
        assertEquals(reporter.crashBreadcrumbs.size, 3)
    }

    @Test
    fun `Reporter stores only max number of breadcrumbs`() {
        val testMessage = "test_Message"
        val testData = hashMapOf("1" to "one", "2" to "two")
        val testCategory = "testing_category"
        val testLevel = Breadcrumb.Level.CRITICAL
        val testType = Breadcrumb.Type.USER

        val service = object : CrashReporterService {
            override fun report(crash: Crash.UncaughtExceptionCrash) {
            }

            override fun report(crash: Crash.NativeCodeCrash) {
            }
        }

        val reporter = spy(CrashReporter(
                services = listOf(service),
                shouldPrompt = CrashReporter.Prompt.NEVER
        ).install(testContext))

        repeat(CrashReporter.BREADCRUMB_MAX_NUM) {
            reporter.recordCrashBreadcrumb(
                    Breadcrumb(testMessage, testData, testCategory, testLevel, testType)
            )
        }
        assertEquals(reporter.crashBreadcrumbs.size, CrashReporter.BREADCRUMB_MAX_NUM)

        reporter.recordCrashBreadcrumb(
                Breadcrumb(testMessage, testData, testCategory, testLevel, testType)
        )
        assertEquals(reporter.crashBreadcrumbs.size, CrashReporter.BREADCRUMB_MAX_NUM)

        reporter.recordCrashBreadcrumb(
                Breadcrumb(testMessage, testData, testCategory, testLevel, testType)
        )
        assertEquals(reporter.crashBreadcrumbs.size, CrashReporter.BREADCRUMB_MAX_NUM)
    }
}
