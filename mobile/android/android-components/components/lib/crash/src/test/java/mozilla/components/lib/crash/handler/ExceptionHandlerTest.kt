/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.crash.handler

import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.ExperimentalCoroutinesApi
import mozilla.components.concept.base.crash.Breadcrumb
import mozilla.components.lib.crash.Crash
import mozilla.components.lib.crash.CrashReporter
import mozilla.components.lib.crash.service.CrashReporterService
import mozilla.components.support.test.any
import mozilla.components.support.test.eq
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify

@ExperimentalCoroutinesApi
@RunWith(AndroidJUnit4::class)
class ExceptionHandlerTest {

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()
    private val scope = coroutinesTestRule.scope

    @Test
    fun `ExceptionHandler forwards crashes to CrashReporter`() {
        val service: CrashReporterService = mock()

        val crashReporter = spy(
            CrashReporter(
                context = testContext,
                shouldPrompt = CrashReporter.Prompt.NEVER,
                services = listOf(service),
                scope = scope,
            ),
        )

        val handler = ExceptionHandler(
            testContext,
            crashReporter,
        )

        val exception = RuntimeException("Hello World")
        handler.uncaughtException(Thread.currentThread(), exception)

        verify(crashReporter).onCrash(eq(testContext), any())
        verify(crashReporter).sendCrashReport(eq(testContext), any())
    }

    @Test
    fun `ExceptionHandler invokes default exception handler`() {
        val defaultExceptionHandler: Thread.UncaughtExceptionHandler = mock()

        val crashReporter = CrashReporter(
            context = testContext,
            shouldPrompt = CrashReporter.Prompt.NEVER,
            services = listOf(
                object : CrashReporterService {
                    override val id: String = "test"

                    override val name: String = "TestReporter"

                    override fun createCrashReportUrl(identifier: String): String? = null

                    override fun report(crash: Crash.UncaughtExceptionCrash): String? = null

                    override fun report(crash: Crash.NativeCodeCrash): String? = null

                    override fun report(throwable: Throwable, breadcrumbs: ArrayList<Breadcrumb>): String? = null
                },
            ),
            scope = scope,
        ).install(testContext)

        val handler = ExceptionHandler(
            testContext,
            crashReporter,
            defaultExceptionHandler,
        )

        verify(defaultExceptionHandler, never()).uncaughtException(any(), any())

        val exception = RuntimeException()
        handler.uncaughtException(Thread.currentThread(), exception)

        verify(defaultExceptionHandler).uncaughtException(Thread.currentThread(), exception)
    }
}
