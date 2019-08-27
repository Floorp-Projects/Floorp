/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.crash.service

import androidx.test.ext.junit.runners.AndroidJUnit4
import io.sentry.SentryClient
import io.sentry.SentryClientFactory
import io.sentry.dsn.Dsn
import mozilla.components.lib.crash.Crash
import mozilla.components.support.test.any
import mozilla.components.support.test.eq
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.never
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
class SentryServiceTest {

    @Test
    fun `SentryService disables exception handler and forwards tags`() {
        var usedDsn: Dsn? = null

        val client: SentryClient = mock()
        val uncaughtExceptionCrash: Crash.UncaughtExceptionCrash = mock()
        val factory = object : SentryClientFactory() {
            override fun createSentryClient(dsn: Dsn?): SentryClient {
                usedDsn = dsn
                return client
            }
        }

        SentryService(
            testContext,
            "https://not:real6@sentry.prod.example.net/405",
            clientFactory = factory,
            tags = mapOf(
                "test" to "world",
                "house" to "boat"
            )).report(uncaughtExceptionCrash)

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
            testContext,
            "https://not:real6@sentry.prod.example.net/405",
            clientFactory = factory)

        val exception = RuntimeException("Hello World")
        service.report(Crash.UncaughtExceptionCrash(exception))

        verify(client).sendException(exception)
    }

    @Test
    fun `SentryService sends message for native code crashes`() {
        val client: SentryClient = mock()

        val service = SentryService(
            testContext,
            "https://not:real6@sentry.prod.example.net/405",
            clientFactory = object : SentryClientFactory() {
                override fun createSentryClient(dsn: Dsn?): SentryClient = client
            },
            sendEventForNativeCrashes = true)

        service.report(Crash.NativeCodeCrash("", true, "", false))

        verify(client).sendMessage(any())
    }

    @Test
    fun `SentryService does not send message for native code crashes by default`() {
        val client: SentryClient = mock()

        val service = SentryService(
            testContext,
            "https://not:real6@sentry.prod.example.net/405",
            clientFactory = object : SentryClientFactory() {
                override fun createSentryClient(dsn: Dsn?): SentryClient = client
            })

        service.report(Crash.NativeCodeCrash("", true, "", false))

        verify(client, never()).sendMessage(any())
    }

    @Test
    fun `SentryService adds default tags`() {
        val client: SentryClient = mock()
        val uncaughtExceptionCrash: Crash.UncaughtExceptionCrash = mock()
        SentryService(
            testContext,
            "https://not:real6@sentry.prod.example.net/405",
            clientFactory = object : SentryClientFactory() {
                override fun createSentryClient(dsn: Dsn?): SentryClient = client
            }).report(uncaughtExceptionCrash)

        verify(client).addTag(eq("ac.version"), any())
        verify(client).addTag(eq("ac.git"), any())
        verify(client).addTag(eq("ac.as.build_version"), any())
    }

    @Test
    fun `SentryService passes an environment or null`() {
        val client: SentryClient = mock()
        val environmentString = "production"
        val uncaughtExceptionCrash: Crash.UncaughtExceptionCrash = mock()
        SentryService(testContext,
                "https://fake:notreal@sentry.prod.example.net/405",
                clientFactory = object : SentryClientFactory() {
                    override fun createSentryClient(dsn: Dsn?): SentryClient = client
                },
                environment = environmentString).report(uncaughtExceptionCrash)
        verify(client).environment = eq(environmentString)

        SentryService(testContext,
                "https://fake:notreal@sentry.prod.example.net/405",
                clientFactory = object : SentryClientFactory() {
                    override fun createSentryClient(dsn: Dsn?): SentryClient = client
                }).report(uncaughtExceptionCrash)
        verify(client).environment = null
    }
}
