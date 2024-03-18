/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.crash

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.concept.base.crash.Breadcrumb
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.spy
import java.lang.Thread.sleep
import java.util.Date

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

        val reporter = spy(
            CrashReporter(
                context = testContext,
                services = listOf(mock()),
                shouldPrompt = CrashReporter.Prompt.NEVER,
                notificationsDelegate = mock(),
            ).install(testContext),
        )

        reporter.recordCrashBreadcrumb(
            Breadcrumb(
                testMessage,
                testData,
                testCategory,
                testLevel,
                testType,
            ),
        )

        reporter.crashBreadcrumbsCopy().elementAt(0).let {
            assertEquals(it.message, testMessage)
            assertEquals(it.data, testData)
            assertEquals(it.category, testCategory)
            assertEquals(it.level, testLevel)
            assertEquals(it.type, testType)
            assertNotNull(it.date)
        }
    }

    @Test
    fun `Reporter stores current number of breadcrumbs`() {
        val testMessage = "test_Message"
        val testData = hashMapOf("1" to "one", "2" to "two")
        val testCategory = "testing_category"
        val testLevel = Breadcrumb.Level.CRITICAL
        val testType = Breadcrumb.Type.USER

        val reporter = spy(
            CrashReporter(
                context = testContext,
                services = listOf(mock()),
                shouldPrompt = CrashReporter.Prompt.NEVER,
                notificationsDelegate = mock(),
            ).install(testContext),
        )

        reporter.recordCrashBreadcrumb(
            Breadcrumb(
                testMessage,
                testData,
                testCategory,
                testLevel,
                testType,
            ),
        )
        assertEquals(reporter.crashBreadcrumbsCopy().size, 1)

        reporter.recordCrashBreadcrumb(
            Breadcrumb(
                testMessage,
                testData,
                testCategory,
                testLevel,
                testType,
            ),
        )
        assertEquals(reporter.crashBreadcrumbsCopy().size, 2)

        reporter.recordCrashBreadcrumb(
            Breadcrumb(
                testMessage,
                testData,
                testCategory,
                testLevel,
                testType,
            ),
        )
        assertEquals(reporter.crashBreadcrumbsCopy().size, 3)
    }

    @Test
    fun `RecordBreadcumb stores correct date`() {
        val testMessage = "test_Message"
        val testData = hashMapOf("1" to "one", "2" to "two")
        val testCategory = "testing_category"
        val testLevel = Breadcrumb.Level.CRITICAL
        val testType = Breadcrumb.Type.USER

        val reporter = spy(
            CrashReporter(
                context = testContext,
                services = listOf(mock()),
                shouldPrompt = CrashReporter.Prompt.NEVER,
                notificationsDelegate = mock(),
            ).install(testContext),
        )

        val beginDate = Date()
        sleep(100) // make sure time elapsed
        reporter.recordCrashBreadcrumb(
            Breadcrumb(
                testMessage,
                testData,
                testCategory,
                testLevel,
                testType,
            ),
        )
        sleep(100) // make sure time elapsed
        val afterDate = Date()

        reporter.crashBreadcrumbsCopy().elementAt(0).let {
            assertTrue(it.date.after(beginDate))
            assertTrue(it.date.before(afterDate))
        }

        val date = Date()
        reporter.recordCrashBreadcrumb(
            Breadcrumb(
                testMessage,
                testData,
                testCategory,
                testLevel,
                testType,
                date,
            ),
        )

        reporter.crashBreadcrumbsCopy().elementAt(1).let {
            assertEquals(it.date.compareTo(date), 0)
        }
    }

    @Test
    fun `Breadcrumb converts correctly to JSON`() {
        val testMessage = "test_Message"
        val testData = hashMapOf("1" to "one", "2" to "two")
        val testCategory = "testing_category"
        val testLevel = Breadcrumb.Level.CRITICAL
        val testType = Breadcrumb.Type.USER
        val testDate = Date(0)
        val testString = "{\"timestamp\":\"1970-01-01T00:00:00\",\"message\":\"test_Message\"," +
            "\"category\":\"testing_category\",\"level\":\"Critical\",\"type\":\"User\"," +
            "\"data\":{\"1\":\"one\",\"2\":\"two\"}}"

        val breadcrumb = Breadcrumb(
            testMessage,
            testData,
            testCategory,
            testLevel,
            testType,
            testDate,
        )
        assertEquals(breadcrumb.toJson().toString(), testString)
    }
}
