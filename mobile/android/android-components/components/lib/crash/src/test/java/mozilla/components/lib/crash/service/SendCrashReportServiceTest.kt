/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.crash.service

import android.content.ComponentName
import android.content.Intent
import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.ExperimentalCoroutinesApi
import mozilla.components.concept.base.crash.Breadcrumb
import mozilla.components.lib.crash.Crash
import mozilla.components.lib.crash.CrashReporter
import mozilla.components.support.test.any
import mozilla.components.support.test.eq
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.fail
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify
import org.robolectric.Robolectric

@ExperimentalCoroutinesApi
@RunWith(AndroidJUnit4::class)
class SendCrashReportServiceTest {
    private var service: SendCrashReportService? = null
    private val intent = Intent("org.mozilla.gecko.ACTION_CRASHED")

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()
    private val scope = coroutinesTestRule.scope

    @Before
    fun setUp() {
        intent.component = ComponentName(
            "org.mozilla.samples.browser",
            "mozilla.components.lib.crash.handler.CrashHandlerService",
        )
        intent.putExtra(
            "minidumpPath",
            "/data/data/org.mozilla.samples.browser/files/mozilla/Crash Reports/pending/3ba5f665-8422-dc8e-a88e-fc65c081d304.dmp",
        )
        intent.putExtra("fatal", false)
        intent.putExtra(
            "extrasPath",
            "/data/data/org.mozilla.samples.browser/files/mozilla/Crash Reports/pending/3ba5f665-8422-dc8e-a88e-fc65c081d304.extra",
        )
        intent.putExtra("minidumpSuccess", true)
        intent.putParcelableArrayListExtra("breadcrumbs", null)
        service = spy(Robolectric.setupService(SendCrashReportService::class.java))
        service?.startService(intent)
    }

    @After
    fun tearDown() {
        service?.stopService(intent)
        CrashReporter.reset()
    }

    @Test
    fun `Send crash report will forward same crash to crash service`() {
        var caughtCrash: Crash.NativeCodeCrash? = null
        val crashReporter = spy(
            CrashReporter(
                context = testContext,
                shouldPrompt = CrashReporter.Prompt.NEVER,
                services = listOf(
                    object : CrashReporterService {
                        override val id: String = "test"

                        override val name: String = "TestReporter"

                        override fun createCrashReportUrl(identifier: String): String? = null

                        override fun report(crash: Crash.UncaughtExceptionCrash): String? {
                            fail("Didn't expect uncaught exception crash")
                            return null
                        }

                        override fun report(crash: Crash.NativeCodeCrash): String? {
                            caughtCrash = crash
                            return null
                        }

                        override fun report(throwable: Throwable, breadcrumbs: ArrayList<Breadcrumb>): String? {
                            fail("Didn't expect caught exception")
                            return null
                        }
                    },
                ),
                scope = scope,
            ),
        ).install(testContext)
        val originalCrash = Crash.NativeCodeCrash(
            123,
            "/data/data/org.mozilla.samples.browser/files/mozilla/Crash Reports/pending/3ba5f665-8422-dc8e-a88e-fc65c081d304.dmp",
            true,
            "/data/data/org.mozilla.samples.browser/files/mozilla/Crash Reports/pending/3ba5f665-8422-dc8e-a88e-fc65c081d304.extra",
            Crash.NativeCodeCrash.PROCESS_TYPE_FOREGROUND_CHILD,
            arrayListOf(),
        )

        val intent = Intent("org.mozilla.gecko.ACTION_CRASHED")
        intent.component = ComponentName(
            "org.mozilla.samples.browser",
            "mozilla.components.lib.crash.handler.CrashHandlerService",
        )
        intent.putExtra(
            "minidumpPath",
            "/data/data/org.mozilla.samples.browser/files/mozilla/Crash Reports/pending/3ba5f665-8422-dc8e-a88e-fc65c081d304.dmp",
        )
        intent.putExtra("processType", "FOREGROUND_CHILD")
        intent.putExtra(
            "extrasPath",
            "/data/data/org.mozilla.samples.browser/files/mozilla/Crash Reports/pending/3ba5f665-8422-dc8e-a88e-fc65c081d304.extra",
        )
        intent.putExtra("minidumpSuccess", true)
        intent.putParcelableArrayListExtra("breadcrumbs", null)
        originalCrash.fillIn(intent)

        service?.onStartCommand(intent, 0, 0)
        verify(crashReporter).submitReport(eq(originalCrash), any())
        assertNotNull(caughtCrash)

        val nativeCrash = caughtCrash
            ?: throw AssertionError("Expected NativeCodeCrash instance")

        assertEquals(123, nativeCrash.timestamp)
        assertEquals(true, nativeCrash.minidumpSuccess)
        assertEquals(false, nativeCrash.isFatal)
        assertEquals(Crash.NativeCodeCrash.PROCESS_TYPE_FOREGROUND_CHILD, nativeCrash.processType)
        assertEquals(
            "/data/data/org.mozilla.samples.browser/files/mozilla/Crash Reports/pending/3ba5f665-8422-dc8e-a88e-fc65c081d304.dmp",
            nativeCrash.minidumpPath,
        )
        assertEquals(
            "/data/data/org.mozilla.samples.browser/files/mozilla/Crash Reports/pending/3ba5f665-8422-dc8e-a88e-fc65c081d304.extra",
            nativeCrash.extrasPath,
        )
    }

    @Test
    fun `notification tag and id is added to the report intent`() {
        val crash: Crash = mock()
        val intent = SendCrashReportService.createReportIntent(testContext, crash, "test_tag", 123)

        assertEquals(intent.getStringExtra(NOTIFICATION_TAG_KEY), "test_tag")
        assertEquals(intent.getIntExtra(NOTIFICATION_ID_KEY, 0), 123)
    }
}
