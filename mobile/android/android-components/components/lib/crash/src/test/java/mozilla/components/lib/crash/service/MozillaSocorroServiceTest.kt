/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.crash.service

import androidx.test.ext.junit.runners.AndroidJUnit4
import com.google.common.io.Resources.getResource
import mozilla.components.lib.crash.Crash
import mozilla.components.support.test.any
import mozilla.components.support.test.robolectric.testContext
import okhttp3.mockwebserver.MockResponse
import okhttp3.mockwebserver.MockWebServer
import org.junit.Assert.assertEquals
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers.anyBoolean
import org.mockito.Mockito.doNothing
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify
import java.io.BufferedReader
import java.io.ByteArrayInputStream
import java.io.File
import java.io.InputStreamReader
import java.util.zip.GZIPInputStream

@RunWith(AndroidJUnit4::class)
class MozillaSocorroServiceTest {

    @Test
    fun `MozillaSocorroService sends native code crashes to GeckoView crash reporter`() {
        val service = spy(MozillaSocorroService(
            testContext,
            "Test App"
        ))
        doNothing().`when`(service).sendReport(any(), any(), any(), anyBoolean())

        val crash = Crash.NativeCodeCrash("", true, "", false, arrayListOf())
        service.report(crash)

        verify(service).report(crash)
        verify(service).sendReport(null, crash.minidumpPath, crash.extrasPath, false)
    }

    @Test
    fun `MozillaSocorroService send uncaught exception crashes`() {
        val service = spy(MozillaSocorroService(
            testContext,
            "Test App"
        ))
        doNothing().`when`(service).sendReport(any(), any(), any(), anyBoolean())

        val crash = Crash.UncaughtExceptionCrash(RuntimeException("Test"), arrayListOf())
        service.report(crash)

        verify(service).report(crash)
        verify(service).sendReport(crash.throwable, null, null, false)
    }

    @Test
    fun `MozillaSocorroService send caught exception`() {
        val service = spy(MozillaSocorroService(
                testContext,
                "Test App"
        ))
        doNothing().`when`(service).sendReport(any(), any(), any(), anyBoolean())

        val throwable = RuntimeException("Test")
        service.report(throwable)

        verify(service).report(throwable)
        verify(service).sendReport(throwable, null, null, true)
    }

    @Test
    fun `MozillaSocorroService uncaught exception request is correct`() {
        var mockWebServer = MockWebServer()
        mockWebServer.enqueue(MockResponse().setResponseCode(200)
                .setBody("CrashID=bp-924121d3-4de3-4b32-ab12-026fc0190928"))
        mockWebServer.start()
        val serverUrl = mockWebServer.url("/")
        val service = spy(MozillaSocorroService(
                testContext,
                "Test App",
                appId = "{aa3c5121-dab2-40e2-81ca-7ea25febc110}",
                serverUrl = serverUrl.toString()
        ))

        val crash = Crash.UncaughtExceptionCrash(RuntimeException("Test"), arrayListOf())
        service.report(crash)

        val fileInputStream = ByteArrayInputStream(mockWebServer.takeRequest().body.inputStream().readBytes())
        val inputStream = GZIPInputStream(fileInputStream)
        val reader = InputStreamReader(inputStream)
        val bufferedReader = BufferedReader(reader)
        var request = bufferedReader.readText()

        assert(request.contains("name=JavaStackTrace\r\n\r\njava.lang.RuntimeException: Test"))
        assert(request.contains("name=Android_ProcessName\r\n\r\nmozilla.components.lib.crash.test"))
        assert(request.contains("name=ProductID\r\n\r\n{aa3c5121-dab2-40e2-81ca-7ea25febc110}"))
        assert(request.contains("name=Vendor\r\n\r\nMozilla"))
        assert(request.contains("name=ReleaseChannel\r\n\r\nnightly"))
        assert(request.contains("name=Android_PackageName\r\n\r\nmozilla.components.lib.crash.test"))
        assert(request.contains("name=Android_Device\r\n\r\nrobolectric"))

        verify(service).report(crash)
        verify(service).sendReport(crash.throwable, null, null, false)
    }

    @Test
    fun `MozillaSocorroService caught exception request is correct`() {
        var mockWebServer = MockWebServer()
        mockWebServer.enqueue(MockResponse().setResponseCode(200)
                .setBody("CrashID=bp-924121d3-4de3-4b32-ab12-026fc0190928"))
        mockWebServer.start()
        val serverUrl = mockWebServer.url("/")
        val service = spy(MozillaSocorroService(
                testContext,
                "Test App",
                serverUrl = serverUrl.toString()
        ))

        val throwable = RuntimeException("Test")
        service.report(throwable)

        val fileInputStream = ByteArrayInputStream(mockWebServer.takeRequest().body.inputStream().readBytes())
        val inputStream = GZIPInputStream(fileInputStream)
        val reader = InputStreamReader(inputStream)
        val bufferedReader = BufferedReader(reader)
        var request = bufferedReader.readText()

        assert(request.contains("name=JavaStackTrace\r\n\r\n$INFO_PREFIX java.lang.RuntimeException: Test"))
        assert(request.contains("name=Android_ProcessName\r\n\r\nmozilla.components.lib.crash.test"))
        assert(request.contains("name=ProductID\r\n\r\n{eeb82917-e434-4870-8148-5c03d4caa81b}"))
        assert(request.contains("name=Vendor\r\n\r\nMozilla"))
        assert(request.contains("name=ReleaseChannel\r\n\r\nnightly"))
        assert(request.contains("name=Android_PackageName\r\n\r\nmozilla.components.lib.crash.test"))
        assert(request.contains("name=Android_Device\r\n\r\nrobolectric"))

        verify(service).report(throwable)
        verify(service).sendReport(throwable, null, null, true)
    }

    @Test
    fun `MozillaSocorroService caught exception request app details are correct`() {
        var mockWebServer = MockWebServer()
        mockWebServer.enqueue(MockResponse().setResponseCode(200)
                .setBody("CrashID=bp-924121d3-4de3-4b32-ab12-026fc0190928"))
        mockWebServer.start()
        val serverUrl = mockWebServer.url("/")
        val service = spy(MozillaSocorroService(
                testContext,
                "Test App",
                "{1234-1234-1234}",
                "0.1",
                "1.0",
                "Mozilla Test",
                serverUrl = serverUrl.toString()
        ))

        val throwable = RuntimeException("Test")
        service.report(throwable)

        val fileInputStream = ByteArrayInputStream(mockWebServer.takeRequest().body.inputStream().readBytes())
        val inputStream = GZIPInputStream(fileInputStream)
        val reader = InputStreamReader(inputStream)
        val bufferedReader = BufferedReader(reader)
        var request = bufferedReader.readText()

        assert(request.contains("name=JavaStackTrace\r\n\r\n$INFO_PREFIX java.lang.RuntimeException: Test"))
        assert(request.contains("name=Android_ProcessName\r\n\r\nmozilla.components.lib.crash.test"))
        assert(request.contains("name=ProductID\r\n\r\n{1234-1234-1234}"))
        assert(request.contains("name=Version\r\n\r\n0.1"))
        assert(request.contains("name=BuildID\r\n\r\n1.0"))
        assert(request.contains("name=Vendor\r\n\r\nMozilla Test"))
        assert(request.contains("name=ReleaseChannel\r\n\r\nnightly"))
        assert(request.contains("name=Android_PackageName\r\n\r\nmozilla.components.lib.crash.test"))
        assert(request.contains("name=Android_Device\r\n\r\nrobolectric"))

        verify(service).report(throwable)
        verify(service).sendReport(throwable, null, null, true)
    }

    @Test
    fun `MozillaSocorroService handles 200 response correctly`() {
        var mockWebServer = MockWebServer()
        mockWebServer.enqueue(MockResponse().setResponseCode(200)
                .setBody("CrashID=bp-924121d3-4de3-4b32-ab12-026fc0190928"))
        mockWebServer.start()
        val serverUrl = mockWebServer.url("/")
        val service = spy(MozillaSocorroService(
                testContext,
                "Test App",
                serverUrl = serverUrl.toString()
        ))

        val crash = Crash.UncaughtExceptionCrash(RuntimeException("Test"), arrayListOf())
        service.report(crash)

        mockWebServer.shutdown()
        verify(service).report(crash)
        verify(service).sendReport(crash.throwable, null, null, false)
    }

    @Test
    fun `MozillaSocorroService handles 404 response correctly`() {
        var mockWebServer = MockWebServer()
        mockWebServer.enqueue(MockResponse().setResponseCode(404).setBody("error"))
        mockWebServer.start()
        val serverUrl = mockWebServer.url("/")
        val service = spy(MozillaSocorroService(
                testContext,
                "Test App",
                serverUrl = serverUrl.toString()
        ))

        val crash = Crash.NativeCodeCrash(null, true, null, false, arrayListOf())
        service.report(crash)
        mockWebServer.shutdown()

        verify(service).report(crash)
        verify(service).sendReport(null, crash.minidumpPath, crash.extrasPath, false)
    }

    @Test
    fun `MozillaSocorroService parses extrasFile correctly`() {
        val service = spy(MozillaSocorroService(
                testContext,
                "Test App"
        ))
        val file = File(getResource("TestExtrasFile").file)
        val extrasMap = service.readExtrasFromFile(file)

        assertEquals(extrasMap.size, 27)
        assertEquals(extrasMap["ContentSandboxLevel"], "2")
        assertEquals(extrasMap["TelemetryEnvironment"], "{\"EscapedField\":\"EscapedData\n\nfoo\"}")
        assertEquals(extrasMap["EMCheckCompatibility"], "true")
        assertEquals(extrasMap["ProductName"], "Firefox")
        assertEquals(extrasMap["ContentSandboxCapabilities"], "119")
        assertEquals(extrasMap["TelemetryClientId"], "")
        assertEquals(extrasMap["Vendor"], "Mozilla")
        assertEquals(extrasMap["InstallTime"], "1000000000")
        assertEquals(extrasMap["Theme"], "classic/1.0")
        assertEquals(extrasMap["ReleaseChannel"], "default")
        assertEquals(extrasMap["ServerURL"], "https://crash-reports.mozilla.com")
        assertEquals(extrasMap["SafeMode"], "0")
        assertEquals(extrasMap["ContentSandboxCapable"], "1")
        assertEquals(extrasMap["useragent_locale"], "en-US")
        assertEquals(extrasMap["Version"], "55.0a1")
        assertEquals(extrasMap["BuildID"], "20170512114708")
        assertEquals(extrasMap["ProductID"], "{ec8030f7-c20a-464f-9b0e-13a3a9e97384}")
        assertEquals(extrasMap["TelemetryServerURL"], "")
        assertEquals(extrasMap["DOMIPCEnabled"], "1")
        assertEquals(extrasMap["Add-ons"], "")
        assertEquals(extrasMap["CrashTime"], "1494582646")
        assertEquals(extrasMap["UptimeTS"], "14.9179586")
        assertEquals(extrasMap["ThreadIdNameMapping"], "")
        assertEquals(extrasMap["ContentSandboxEnabled"], "1")
        assertEquals(extrasMap["StartupTime"], "1000000000")
        assertEquals(extrasMap["URL"], "about:home")
    }

    @Test
    fun `MozillaSocorroService parses legacyExtraFile correctly`() {
        val service = spy(MozillaSocorroService(
                testContext,
                "Test App"
        ))
        val file = File(getResource("TestLegacyExtrasFile").file)
        val extrasMap = service.readExtrasFromFile(file)

        assertEquals(extrasMap.size, 27)
        assertEquals(extrasMap["ContentSandboxLevel"], "2")
        assertEquals(extrasMap["TelemetryEnvironment"], "{\"EscapedField\":\"EscapedData\n\nfoo\"}")
        assertEquals(extrasMap["EMCheckCompatibility"], "true")
        assertEquals(extrasMap["ProductName"], "Firefox")
        assertEquals(extrasMap["ContentSandboxCapabilities"], "119")
        assertEquals(extrasMap["TelemetryClientId"], "")
        assertEquals(extrasMap["Vendor"], "Mozilla")
        assertEquals(extrasMap["InstallTime"], "1000000000")
        assertEquals(extrasMap["Theme"], "classic/1.0")
        assertEquals(extrasMap["ReleaseChannel"], "default")
        assertEquals(extrasMap["ServerURL"], "https://crash-reports.mozilla.com")
        assertEquals(extrasMap["SafeMode"], "0")
        assertEquals(extrasMap["ContentSandboxCapable"], "1")
        assertEquals(extrasMap["useragent_locale"], "en-US")
        assertEquals(extrasMap["Version"], "55.0a1")
        assertEquals(extrasMap["BuildID"], "20170512114708")
        assertEquals(extrasMap["ProductID"], "{ec8030f7-c20a-464f-9b0e-13a3a9e97384}")
        assertEquals(extrasMap["TelemetryServerURL"], "")
        assertEquals(extrasMap["DOMIPCEnabled"], "1")
        assertEquals(extrasMap["Add-ons"], "")
        assertEquals(extrasMap["CrashTime"], "1494582646")
        assertEquals(extrasMap["UptimeTS"], "14.9179586")
        assertEquals(extrasMap["ThreadIdNameMapping"], "")
        assertEquals(extrasMap["ContentSandboxEnabled"], "1")
        assertEquals(extrasMap["StartupTime"], "1000000000")
        assertEquals(extrasMap["URL"], "about:home")
    }

    @Test
    fun `MozillaSocorroService handles bad extrasFile correctly`() {
        val service = spy(MozillaSocorroService(
                testContext,
                "Test App"
        ))
        val file = File(getResource("BadTestExtrasFile").file)
        val extrasMap = service.readExtrasFromFile(file)

        assertEquals(extrasMap.size, 0)
    }

    @Test
    fun `MozillaSocorroService unescape strings correctly`() {
        val service = spy(MozillaSocorroService(
                testContext,
                "Test App"
        ))
        val test1 = "\\\\\\\\"
        val expected1 = "\\"
        assert(service.unescape(test1) == expected1)

        val test2 = "\\\\n"
        val expected2 = "\n"
        assert(service.unescape(test2) == expected2)

        val test3 = "\\\\t"
        val expected3 = "\t"
        assert(service.unescape(test3) == expected3)

        val test4 = "\\\\\\\\\\\\t\\\\t\\\\n\\\\\\\\"
        val expected4 = "\\\t\t\n\\"
        assert(service.unescape(test4) == expected4)
    }
}
