/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.metrics

import androidx.navigation.NavController
import androidx.navigation.NavDestination
import mozilla.components.lib.crash.Crash
import mozilla.components.lib.crash.CrashReporter
import mozilla.components.lib.crash.service.CrashReporterService
import mozilla.components.support.base.crash.Breadcrumb
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import org.junit.Test
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify

internal class BreadcrumbRecorderTest {
    @Test
    fun `ensure crash reporter recordCrashBreadcrumb is called`() {
        val service = object : CrashReporterService {
            override fun report(throwable: Throwable, breadcrumbs: ArrayList<Breadcrumb>): String? = ""
            override fun report(crash: Crash.NativeCodeCrash): String? = ""
            override fun report(crash: Crash.UncaughtExceptionCrash): String? = ""
        }

        val reporter = spy(
            CrashReporter(
                services = listOf(service),
                shouldPrompt = CrashReporter.Prompt.NEVER
            )
        )

        fun getBreadcrumbMessage(@Suppress("UNUSED_PARAMETER") destination: NavDestination): String {
            return "test"
        }

        val navController: NavController = mock()
        val navDestination: NavDestination = mock()

        val breadCrumbRecorder =
            BreadcrumbsRecorder(reporter, navController, ::getBreadcrumbMessage)
        breadCrumbRecorder.onDestinationChanged(navController, navDestination, null)

        verify(reporter).recordCrashBreadcrumb(any())
    }
}
