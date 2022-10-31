/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.crash.sentry.legacy

import androidx.test.ext.junit.runners.AndroidJUnit4
import io.sentry.SentryClient
import io.sentry.SentryClientFactory
import io.sentry.context.Context
import io.sentry.dsn.Dsn
import io.sentry.event.Event
import io.sentry.event.EventBuilder
import mozilla.components.concept.base.crash.Breadcrumb
import mozilla.components.lib.crash.Crash
import mozilla.components.lib.crash.CrashReporter
import mozilla.components.support.test.any
import mozilla.components.support.test.eq
import mozilla.components.support.test.ext.isLazyInitialized
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentCaptor
import org.mockito.ArgumentMatchers
import org.mockito.Mockito.doNothing
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
import org.mockito.Mockito.times
import org.mockito.Mockito.verify
import org.mockito.Mockito.`when`
import java.util.Date

@RunWith(AndroidJUnit4::class)
class SentryServiceTest {

    @Test
    fun `SentryService disables exception handler and forwards tags`() {
        var usedDsn: Dsn? = null

        val client: SentryClient = mock()
        val uncaughtExceptionCrash = Crash.UncaughtExceptionCrash(0, RuntimeException("test"), arrayListOf())
        val factory = object : SentryClientFactory() {
            override fun createSentryClient(dsn: Dsn?): SentryClient {
                usedDsn = dsn
                return client
            }
        }
        val clientContext: Context = mock()
        doReturn(clientContext).`when`(client).context
        doNothing().`when`(clientContext).clearBreadcrumbs()

        SentryService(
            testContext,
            "https://not:real6@sentry.prod.example.net/405",
            clientFactory = factory,
            tags = mapOf(
                "test" to "world",
                "house" to "boat",
            ),
        ).report(uncaughtExceptionCrash)

        assertNotNull(usedDsn)

        val options = usedDsn!!.options

        assertTrue(options.containsKey("uncaught.handler.enabled"))
        assertEquals("false", options.get("uncaught.handler.enabled"))

        verify(client).addTag("test", "world")
        verify(client).addTag("house", "boat")
        verify(clientContext).clearBreadcrumbs()
    }

    @Test
    fun `SentryService forwards uncaught exception to client`() {
        val client: SentryClient = mock()
        val clientContext: Context = mock()
        doReturn(clientContext).`when`(client).context
        doNothing().`when`(clientContext).clearBreadcrumbs()

        val factory = object : SentryClientFactory() {
            override fun createSentryClient(dsn: Dsn?): SentryClient = client
        }

        val service = SentryService(
            testContext,
            "https://not:real6@sentry.prod.example.net/405",
            clientFactory = factory,
        )

        val exception = RuntimeException("Hello World")
        service.report(Crash.UncaughtExceptionCrash(0, exception, arrayListOf()))

        verify(client).sendEvent(ArgumentMatchers.any(EventBuilder::class.java))
        verify(clientContext).clearBreadcrumbs()
    }

    @Test
    fun `SentryService forwards caught exception to client`() {
        val client: SentryClient = mock()
        val breadcrumbs: ArrayList<Breadcrumb> = arrayListOf()
        val clientContext: Context = mock()
        doReturn(clientContext).`when`(client).context
        doNothing().`when`(clientContext).clearBreadcrumbs()

        val factory = object : SentryClientFactory() {
            override fun createSentryClient(dsn: Dsn?): SentryClient = client
        }

        val service = SentryService(
            testContext,
            "https://not:real6@sentry.prod.example.net/405",
            clientFactory = factory,
        )

        val exception = RuntimeException("Hello World")
        service.report(exception, breadcrumbs)

        verify(client).sendEvent(ArgumentMatchers.any(EventBuilder::class.java))
        verify(clientContext).clearBreadcrumbs()
    }

    @Test
    fun `SentryService sends event for native code crashes`() {
        val client: SentryClient = mock()
        val clientContext: Context = mock()
        doReturn(clientContext).`when`(client).context
        doNothing().`when`(clientContext).clearBreadcrumbs()

        val service = SentryService(
            testContext,
            "https://not:real6@sentry.prod.example.net/405",
            clientFactory = object : SentryClientFactory() {
                override fun createSentryClient(dsn: Dsn?): SentryClient = client
            },
            sendEventForNativeCrashes = true,
        )

        service.report(Crash.NativeCodeCrash(0, "", true, "", Crash.NativeCodeCrash.PROCESS_TYPE_FOREGROUND_CHILD, arrayListOf()))

        verify(client).sendEvent(any<EventBuilder>())
        verify(clientContext).clearBreadcrumbs()
    }

    @Test
    fun `Main process native code crashes are reported as level FATAL`() {
        val client: SentryClient = mock()
        val clientContext: Context = mock()
        doReturn(clientContext).`when`(client).context
        doNothing().`when`(clientContext).clearBreadcrumbs()

        val service = SentryService(
            testContext,
            "https://not:real6@sentry.prod.example.net/405",
            clientFactory = object : SentryClientFactory() {
                override fun createSentryClient(dsn: Dsn?): SentryClient = client
            },
            sendEventForNativeCrashes = true,
        )

        service.report(Crash.NativeCodeCrash(0, "", true, "", Crash.NativeCodeCrash.PROCESS_TYPE_MAIN, arrayListOf()))

        val eventBuilderCaptor: ArgumentCaptor<EventBuilder> = ArgumentCaptor.forClass(EventBuilder::class.java)
        verify(client, times(1)).sendEvent(eventBuilderCaptor.capture())

        val capturedEvent: List<EventBuilder> = eventBuilderCaptor.allValues
        assertEquals(Event.Level.FATAL, capturedEvent[0].event.level)
        verify(clientContext).clearBreadcrumbs()
    }

    @Test
    fun `Foreground child process native code crashes are reported as level ERROR`() {
        val client: SentryClient = mock()
        val clientContext: Context = mock()
        doReturn(clientContext).`when`(client).context
        doNothing().`when`(clientContext).clearBreadcrumbs()

        val service = SentryService(
            testContext,
            "https://not:real6@sentry.prod.example.net/405",
            clientFactory = object : SentryClientFactory() {
                override fun createSentryClient(dsn: Dsn?): SentryClient = client
            },
            sendEventForNativeCrashes = true,
        )

        service.report(Crash.NativeCodeCrash(0, "", true, "", Crash.NativeCodeCrash.PROCESS_TYPE_FOREGROUND_CHILD, arrayListOf()))

        val eventBuilderCaptor: ArgumentCaptor<EventBuilder> = ArgumentCaptor.forClass(EventBuilder::class.java)
        verify(client, times(1)).sendEvent(eventBuilderCaptor.capture())

        val capturedEvent: List<EventBuilder> = eventBuilderCaptor.allValues
        assertEquals(Event.Level.ERROR, capturedEvent[0].event.level)
        verify(clientContext).clearBreadcrumbs()
    }

    @Test
    fun `Background child process native code crashes are reported as level ERROR`() {
        val client: SentryClient = mock()
        val clientContext: Context = mock()
        doReturn(clientContext).`when`(client).context
        doNothing().`when`(clientContext).clearBreadcrumbs()

        val service = SentryService(
            testContext,
            "https://not:real6@sentry.prod.example.net/405",
            clientFactory = object : SentryClientFactory() {
                override fun createSentryClient(dsn: Dsn?): SentryClient = client
            },
            sendEventForNativeCrashes = true,
        )

        service.report(Crash.NativeCodeCrash(0, "", true, "", Crash.NativeCodeCrash.PROCESS_TYPE_BACKGROUND_CHILD, arrayListOf()))

        val eventBuilderCaptor: ArgumentCaptor<EventBuilder> = ArgumentCaptor.forClass(EventBuilder::class.java)
        verify(client, times(1)).sendEvent(eventBuilderCaptor.capture())

        val capturedEvent: List<EventBuilder> = eventBuilderCaptor.allValues
        assertEquals(Event.Level.ERROR, capturedEvent[0].event.level)
        verify(clientContext).clearBreadcrumbs()
    }

    @Test
    fun `uncaught exception crashes are reported as level FATAL`() {
        val client: SentryClient = mock()
        val clientContext: Context = mock()
        doReturn(clientContext).`when`(client).context
        doNothing().`when`(clientContext).clearBreadcrumbs()

        val service = SentryService(
            testContext,
            "https://not:real6@sentry.prod.example.net/405",
            clientFactory = object : SentryClientFactory() {
                override fun createSentryClient(dsn: Dsn?): SentryClient = client
            },
        )

        service.report(Crash.UncaughtExceptionCrash(0, RuntimeException("test"), arrayListOf()))

        val eventBuilderCaptor: ArgumentCaptor<EventBuilder> = ArgumentCaptor.forClass(EventBuilder::class.java)
        verify(client, times(1)).sendEvent(eventBuilderCaptor.capture())

        val capturedEvent: List<EventBuilder> = eventBuilderCaptor.allValues
        assertEquals(Event.Level.FATAL, capturedEvent[0].event.level)
        verify(clientContext).clearBreadcrumbs()
    }

    @Test
    fun `caught exception crashes are reported as level INFO`() {
        val client: SentryClient = mock()
        val clientContext: Context = mock()
        doReturn(clientContext).`when`(client).context
        doNothing().`when`(clientContext).clearBreadcrumbs()

        val service = SentryService(
            testContext,
            "https://not:real6@sentry.prod.example.net/405",
            clientFactory = object : SentryClientFactory() {
                override fun createSentryClient(dsn: Dsn?): SentryClient = client
            },
        )

        val throwable = RuntimeException("Test")
        val crashBreadCrumbs = arrayListOf<Breadcrumb>()
        service.report(throwable, crashBreadCrumbs)

        val eventBuilderCaptor: ArgumentCaptor<EventBuilder> = ArgumentCaptor.forClass(EventBuilder::class.java)
        verify(client, times(1)).sendEvent(eventBuilderCaptor.capture())

        val capturedEvent: List<EventBuilder> = eventBuilderCaptor.allValues
        assertEquals(Event.Level.INFO, capturedEvent[0].event.level)
        verify(clientContext).clearBreadcrumbs()
    }

    @Test
    fun `SentryService does not send event for native code crashes by default`() {
        val client: SentryClient = mock()

        val service = SentryService(
            testContext,
            "https://not:real6@sentry.prod.example.net/405",
            clientFactory = object : SentryClientFactory() {
                override fun createSentryClient(dsn: Dsn?): SentryClient = client
            },
        )

        service.report(Crash.NativeCodeCrash(0, "", true, "", Crash.NativeCodeCrash.PROCESS_TYPE_FOREGROUND_CHILD, arrayListOf()))

        verify(client, never()).sendEvent(any<EventBuilder>())
    }

    @Test
    fun `SentryService adds default tags`() {
        val client: SentryClient = mock()
        val uncaughtExceptionCrash = Crash.UncaughtExceptionCrash(0, RuntimeException("test"), arrayListOf())
        val clientContext: Context = mock()
        doReturn(clientContext).`when`(client).context
        doNothing().`when`(clientContext).clearBreadcrumbs()

        SentryService(
            testContext,
            "https://not:real6@sentry.prod.example.net/405",
            clientFactory = object : SentryClientFactory() {
                override fun createSentryClient(dsn: Dsn?): SentryClient = client
            },
        ).report(uncaughtExceptionCrash)

        verify(client).addTag(eq("ac.version"), any())
        verify(client).addTag(eq("ac.git"), any())
        verify(client).addTag(eq("ac.as.build_version"), any())
        verify(client).addTag(eq("ac.glean.build_version"), any())
        verify(client).addTag(eq("user.locale"), any())
        verify(clientContext).clearBreadcrumbs()
    }

    @Test
    fun `SentryService passes an environment or null`() {
        val client: SentryClient = mock()
        val clientContext: Context = mock()
        val environmentString = "production"
        val uncaughtExceptionCrash = Crash.UncaughtExceptionCrash(0, RuntimeException("test"), arrayListOf())
        doReturn(clientContext).`when`(client).context
        doNothing().`when`(clientContext).clearBreadcrumbs()

        SentryService(
            testContext,
            "https://fake:notreal@sentry.prod.example.net/405",
            clientFactory = object : SentryClientFactory() {
                override fun createSentryClient(dsn: Dsn?): SentryClient = client
            },
            environment = environmentString,
        ).report(uncaughtExceptionCrash)
        verify(client).sendEvent(any<EventBuilder>())
        verify(client).environment = eq(environmentString)
        verify(clientContext).clearBreadcrumbs()

        SentryService(
            testContext,
            "https://fake:notreal@sentry.prod.example.net/405",
            clientFactory = object : SentryClientFactory() {
                override fun createSentryClient(dsn: Dsn?): SentryClient = client
            },
        ).report(uncaughtExceptionCrash)
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
            sendEventForNativeCrashes = true,
        )

        val reporter = spy(
            CrashReporter(
                context = testContext,
                services = listOf(service),
                shouldPrompt = CrashReporter.Prompt.NEVER,
            ).install(testContext),
        )

        `when`(client.context).thenReturn(clientContext)

        reporter.recordCrashBreadcrumb(
            Breadcrumb(
                testMessage,
                testData,
                testCategory,
                testLevel,
                testType,
            ),
        )
        val crashBreadCrumbs = arrayListOf<Breadcrumb>()
        crashBreadCrumbs.addAll(reporter.crashBreadcrumbsCopy())
        val nativeCrash = Crash.NativeCodeCrash(
            0,
            "dump.path",
            true,
            "extras.path",
            processType = Crash.NativeCodeCrash.PROCESS_TYPE_FOREGROUND_CHILD,
            breadcrumbs = crashBreadCrumbs,
        )

        service.report(nativeCrash)
        verify(clientContext).recordBreadcrumb(any())
    }

    @Test
    fun `SentryService records breadcrumb caught exception report is called`() {
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
            sendEventForNativeCrashes = true,
        )

        val reporter = spy(
            CrashReporter(
                testContext,
                services = listOf(service),
                shouldPrompt = CrashReporter.Prompt.NEVER,
            ).install(testContext),
        )

        `when`(client.context).thenReturn(clientContext)

        reporter.recordCrashBreadcrumb(
            Breadcrumb(
                testMessage,
                testData,
                testCategory,
                testLevel,
                testType,
            ),
        )
        val throwable = RuntimeException("Test")
        val crashBreadCrumbs = arrayListOf<Breadcrumb>()
        crashBreadCrumbs.addAll(reporter.crashBreadcrumbsCopy())

        service.report(throwable, crashBreadCrumbs)
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
            sendEventForNativeCrashes = true,
        )

        val breadcrumb = Breadcrumb(
            testMessage,
            testData,
            testCategory,
            testLevel,
            testType,
            testDate,
        )
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

    @Test
    fun `SentryService#client is lazily instantiated (for Fenix perf reasons)`() {
        val service = SentryService(
            testContext,
            "https://not:real6@sentry.prod.example.net/405",
        )

        assertFalse(service::client.isLazyInitialized)
    }
}
