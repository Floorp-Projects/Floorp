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
                serverUrl.toString()
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
                serverUrl.toString()
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
        assert(request.contains("name=ProductID\r\n\r\n{aa3c5121-dab2-40e2-81ca-7ea25febc110}"))
        assert(request.contains("name=Vendor\r\n\r\nMozilla"))
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
                serverUrl.toString()
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
                serverUrl.toString()
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

        assert(extrasMap.size == 9)
        assert(extrasMap["InstallTime"] == "1569440259")
        assert(extrasMap["ProductName"] == "Test Product")
        assert(extrasMap["StartupCrash"] == "0")
        assert(extrasMap["StartupTime"] == "1569642043")
        assert(extrasMap["useragent_locale"] == "en-US")
        assert(extrasMap["CrashTime"] == "1569642098")
        assert(extrasMap["UptimeTS"] == "56.3663876")
        assert(extrasMap["SecondsSinceLastCrash"] == "104")
        assert(extrasMap["TextureUsage"] == "12345678")
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
