package org.mozilla.fenix.ext

import kotlinx.coroutines.ObsoleteCoroutinesApi
import mozilla.components.support.test.robolectric.testContext
import org.mozilla.fenix.TestApplication
import org.junit.Test
import org.junit.runner.RunWith
import org.junit.Assert.assertEquals
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config

@ObsoleteCoroutinesApi
@RunWith(RobolectricTestRunner::class)
@Config(application = TestApplication::class)

class StringTest {

    @Test
    fun `Replace`() {
        val chars = mapOf("Mozilla Corporation" to "moco", "Mozilla Foundation" to "mofo")
        var sentence = "Mozilla Corporation and Mozilla Foundation are committed to the future of the internet"
        val new = sentence.replace(chars)
        assertEquals(new, "moco and mofo are committed to the future of the internet")
    }

    @Test
    fun `Get Host From Url`() {
        val urlTest = "http://www.example.com:1080/docs/resource1.html"
        var new = urlTest.getHostFromUrl()
        assertEquals(new, "www.example.com")
    }

    @Test
    fun `Url To Trimmed Host`() {
        val urlTest = "http://www.example.com:1080/docs/resource1.html"
        var new = urlTest.urlToTrimmedHost(testContext)
        assertEquals(new, "example")
    }

    @Test
    fun `Simplified Url`() {
        val urlTest = "https://www.amazon.com"
        var new = urlTest.simplifiedUrl()
        assertEquals(new, "amazon.com")
    }
}
