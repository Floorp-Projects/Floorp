/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.crash.service

import androidx.test.ext.junit.runners.AndroidJUnit4
import io.sentry.SentryClient
import io.sentry.SentryClientFactory
import io.sentry.context.Context
import io.sentry.dsn.Dsn
import mozilla.components.lib.crash.Crash
import mozilla.components.lib.crash.Breadcrumb
import mozilla.components.lib.crash.CrashReporter
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
import org.mockito.Mockito.spy
import org.mockito.Mockito.`when`
import java.util.Date

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
        service.report(Crash.UncaughtExceptionCrash(exception, arrayListOf()))

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

        service.report(Crash.NativeCodeCrash("", true, "", false, arrayListOf()))

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

        service.report(Crash.NativeCodeCrash("", true, "", false, arrayListOf()))

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

    @Test
    fun `SentryService records breadcrumb when CrashReporterService report is called`() {
        val client: SentryClient = mock()
        val clientContext: Context = mock()
        val testMessage = "test_Message"
        val testData = hashMapOf("1" to "one", "2" to "two")
        val testCategory = "testing_category"
        val testLevel = Breadcrumb.Level.CRITICAL
        val testType = Breadcrumb.Type.USER

        val factory = object : SentryClientFactory() {
            override fun createSentryClient(dsn: Dsn?): SentryClient {
                return client
            }
        }

        val service = SentryService(
                testContext,
                "https://not:real6@sentry.prod.example.net/405",
                clientFactory = factory,
                sendEventForNativeCrashes = true
                )

        val reporter = spy(CrashReporter(
                services = listOf(service),
                shouldPrompt = CrashReporter.Prompt.NEVER
        ).install(testContext))

        `when`(client.context).thenReturn(clientContext)

        reporter.recordCrashBreadcrumb(
                Breadcrumb(testMessage, testData, testCategory, testLevel, testType)
        )
        var crashBreadCrumbs = arrayListOf<Breadcrumb>()
        crashBreadCrumbs.addAll(reporter.crashBreadcrumbs)
        val nativeCrash = Crash.NativeCodeCrash(
                "dump.path",
                true,
                "extras.path",
                isFatal = false,
                breadcrumbs = crashBreadCrumbs)

        service.report(nativeCrash)
        verify(clientContext).recordBreadcrumb(any())
    }

    @Test
    fun `Confirm Sentry breadcrumb conversion`() {
        val client: SentryClient = mock()
        val testMessage = "test_Message"
        val testData = hashMapOf("1" to "one", "2" to "two")
        val testCategory = "testing_category"
        val testLevel = Breadcrumb.Level.CRITICAL
        val testType = Breadcrumb.Type.USER
        val testDate = Date()

        val factory = object : SentryClientFactory() {
            override fun createSentryClient(dsn: Dsn?): SentryClient {
                return client
            }
        }

        val service = SentryService(
                testContext,
                "https://not:real6@sentry.prod.example.net/405",
                clientFactory = factory,
                sendEventForNativeCrashes = true
        )

        val breadcrumb = Breadcrumb(testMessage, testData, testCategory, testLevel, testType, testDate)
        service.apply {
            val sentryBreadCrumb = breadcrumb.toSentryBreadcrumb()
            assertTrue(sentryBreadCrumb.message == testMessage)
            assertTrue(sentryBreadCrumb.data == testData)
            assertTrue(sentryBreadCrumb.category == testCategory)
            assertTrue(sentryBreadCrumb.level == testLevel.toSentryBreadcrumbLevel())
            assertTrue(sentryBreadCrumb.type == testType.toSentryBreadcrumbType())
            assertTrue(sentryBreadCrumb.timestamp.compareTo(testDate) == 0)
        }
    }
}
