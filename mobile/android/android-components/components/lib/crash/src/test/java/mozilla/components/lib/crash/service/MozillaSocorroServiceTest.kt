/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.crash.service

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.lib.crash.Crash
import mozilla.components.support.test.any
import mozilla.components.support.test.robolectric.testContext
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.doNothing
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify
import org.mockito.Mockito.verifyNoMoreInteractions

@RunWith(AndroidJUnit4::class)
class MozillaSocorroServiceTest {

    @Test
    fun `MozillaSocorroService sends native code crashes to GeckoView crash reporter`() {
        val service = spy(MozillaSocorroService(
            testContext,
            "Test App"
        ))
        doNothing().`when`(service).sendViaGeckoViewCrashReporter(any())

        val crash = Crash.NativeCodeCrash("", true, "", false, arrayListOf())
        service.report(crash)

        verify(service).report(crash)
        verify(service).sendViaGeckoViewCrashReporter(crash)
    }

    @Test
    fun `MozillaSocorroService does not send uncaught exception crashes`() {
        val service = spy(MozillaSocorroService(
            testContext,
            "Test App"
        ))
        doNothing().`when`(service).sendViaGeckoViewCrashReporter(any())

        val crash = Crash.UncaughtExceptionCrash(RuntimeException("Test"), arrayListOf())
        service.report(crash)

        verify(service).report(crash)
        verifyNoMoreInteractions(service)
    }
}
