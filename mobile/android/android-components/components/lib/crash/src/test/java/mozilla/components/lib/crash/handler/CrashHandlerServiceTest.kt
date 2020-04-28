/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.crash.handler

import android.content.ComponentName
import android.content.Intent
import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.TestCoroutineDispatcher
import kotlinx.coroutines.test.TestCoroutineScope
import mozilla.components.lib.crash.CrashReporter
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.After
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.doNothing
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify

@ExperimentalCoroutinesApi
@RunWith(AndroidJUnit4::class)
class CrashHandlerServiceTest {
    private val testDispatcher = TestCoroutineDispatcher()
    private var service: CrashHandlerService? = null
    private var reporter: CrashReporter? = null
    private var intent: Intent? = null

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule(testDispatcher)

    @Before
    fun setUp() {
        val scope = TestCoroutineScope(testDispatcher)
        reporter = spy(CrashReporter(
            shouldPrompt = CrashReporter.Prompt.NEVER,
            services = listOf(mock()),
            nonFatalCrashIntent = mock(),
            scope = scope
        )).install(testContext)

        intent = Intent("org.mozilla.gecko.ACTION_CRASHED")
        intent?.component = ComponentName(
            "org.mozilla.samples.browser",
            "mozilla.components.lib.crash.handler.CrashHandlerService"
        )
        intent?.putExtra(
            "uuid",
            "94f66ed7-50c7-41d1-96a7-299139a8c2af"
        )
        intent?.putExtra(
            "minidumpPath",
            "/data/data/org.mozilla.samples.browser/files/mozilla/Crash Reports/pending/3ba5f665-8422-dc8e-a88e-fc65c081d304.dmp"
        )
        intent?.putExtra(
            "extrasPath",
            "/data/data/org.mozilla.samples.browser/files/mozilla/Crash Reports/pending/3ba5f665-8422-dc8e-a88e-fc65c081d304.extra"
        )
        intent?.putExtra("minidumpSuccess", true)
        service = spy(CrashHandlerService())
    }

    @After
    fun tearDown() {
        CrashReporter.reset()
    }

    @Test
    fun `CrashHandlerService forwards fatal native code crash to crash reporter`() {
        doNothing().`when`(reporter)?.sendCrashReport(any(), any())
        doNothing().`when`(service)?.kill()

        intent?.putExtra("fatal", true)
        service?.onHandleIntent(intent)
        verify(reporter)?.onCrash(any(), any())
        verify(reporter)?.sendCrashReport(any(), any())
        verify(reporter, never())?.sendNonFatalCrashIntent(any(), any())
    }

    @Test
    fun `CrashHandlerService forwards non-fatal native code crash to crash reporter`() {
        doNothing().`when`(reporter)?.sendCrashReport(any(), any())
        doNothing().`when`(service)?.kill()

        intent?.putExtra("fatal", false)
        service?.onHandleIntent(intent)
        verify(reporter)?.onCrash(any(), any())
        verify(reporter)?.sendNonFatalCrashIntent(any(), any())
        verify(reporter, never())?.sendCrashReport(any(), any())
    }
}
