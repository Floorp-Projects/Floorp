/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.crash.service

import android.content.Context
import androidx.test.core.app.ApplicationProvider
import io.sentry.SentryClient
import io.sentry.SentryClientFactory
import io.sentry.dsn.Dsn
import mozilla.components.lib.crash.Crash
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.never
import org.mockito.Mockito.verify
import org.mockito.Mockito.verifyNoMoreInteractions
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class SentryServiceTest {
    private val context: Context
        get() = ApplicationProvider.getApplicationContext()

    @Test
    fun `SentryService disables exception handler and forwards tags`() {
        var usedDsn: Dsn? = null

        val client: SentryClient = mock()

        val factory = object : SentryClientFactory() {
            override fun createSentryClient(dsn: Dsn?): SentryClient {
                usedDsn = dsn
                return client
            }
        }

        SentryService(
            context,
            "https://not:real6@sentry.prod.example.net/405",
            clientFactory = factory,
            tags = mapOf(
                "test" to "world",
                "house" to "boat"
            ))

        assertNotNull(usedDsn)

        val options = usedDsn!!.options

        assertTrue(options.containsKey("uncaught.handler.enabled"))
        assertEquals("false", options.get("uncaught.handler.enabled"))

        verify(client).addTag("test", "world")
        verify(client).addTag("house", "boat")
    }

    @Test
    fun `SentryService forwards uncaught exceptions to client`() {
        val client: SentryClient = mock()

        val factory = object : SentryClientFactory() {
            override fun createSentryClient(dsn: Dsn?): SentryClient = client
        }

        val service = SentryService(
            context,
            "https://not:real6@sentry.prod.example.net/405",
            clientFactory = factory)

        val exception = RuntimeException("Hello World")
        service.report(Crash.UncaughtExceptionCrash(exception))

        verify(client).sendException(exception)
        verifyNoMoreInteractions(client)
    }

    @Test
    fun `SentryService sends message for native code crashes`() {
        val client: SentryClient = mock()

        val service = SentryService(
            context,
            "https://not:real6@sentry.prod.example.net/405",
            clientFactory = object : SentryClientFactory() {
                override fun createSentryClient(dsn: Dsn?): SentryClient = client
            },
            sendEventForNativeCrashes = true)

        service.report(Crash.NativeCodeCrash("", true, "", false))

        verify(client).sendMessage(any())
        verifyNoMoreInteractions(client)
    }

    @Test
    fun `SentryService does not send message for native code crashes by default`() {
        val client: SentryClient = mock()

        val service = SentryService(
            context,
            "https://not:real6@sentry.prod.example.net/405",
            clientFactory = object : SentryClientFactory() {
                override fun createSentryClient(dsn: Dsn?): SentryClient = client
            })

        service.report(Crash.NativeCodeCrash("", true, "", false))

        verify(client, never()).sendMessage(any())
        verifyNoMoreInteractions(client)
    }
}
