/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.fretboard.source.kinto

import mozilla.components.concept.fetch.Client
import mozilla.components.concept.fetch.MutableHeaders
import mozilla.components.concept.fetch.Response
import mozilla.components.service.fretboard.ExperimentDownloadException
import mozilla.components.support.test.any
import mozilla.components.support.test.eq
import org.junit.Assert.assertEquals
import org.junit.Test
import org.mockito.Mockito
import org.mockito.Mockito.`when`
import org.mockito.Mockito.mock
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify
import java.io.IOException

class KintoClientTest {
    private val baseUrl = "http://example.test"
    private val bucketName = "fretboard"
    private val collectionName = "experiments"

    @Test
    fun get() {
        val httpClient = mock(Client::class.java)
        `when`(httpClient.fetch(any())).thenReturn(
            Response("", 200, MutableHeaders(), Response.Body("getResult".byteInputStream()))
        )

        val kintoClient = KintoClient(httpClient, baseUrl, bucketName, collectionName)
        assertEquals("getResult", kintoClient.get())
    }

    @Test
    fun diff() {
        val httpClient = mock(Client::class.java)
        `when`(httpClient.fetch(any())).thenReturn(
                Response("", 200, MutableHeaders(), Response.Body("diffResult".byteInputStream()))
        )

        val kintoClient = spy(KintoClient(httpClient, baseUrl, bucketName, collectionName))
        assertEquals("diffResult", kintoClient.diff(1527179995))

        verify(kintoClient).fetch(eq("http://example.test/buckets/fretboard/collections/experiments/records?_since=1527179995"))
    }

    /**
     * On some devices we are seeing an ArrayIndexOutOfBoundsException somewhere inside AOSP/okhttp.
     *
     * See:
     * https://github.com/mozilla-mobile/android-components/issues/964
     */
    @Test(expected = ExperimentDownloadException::class)
    fun handlesArrayIndexOutOfBoundsExceptioForFetch() {
        val httpClient = mock(Client::class.java)
        val kintoClient = KintoClient(httpClient, baseUrl, bucketName, collectionName)

        Mockito.doThrow(ArrayIndexOutOfBoundsException()).`when`(httpClient).fetch(any())

        kintoClient.fetch("test")
    }

    @Test(expected = ExperimentDownloadException::class)
    fun handlesIOExceptionForFetch() {
        val httpClient = mock(Client::class.java)
        val kintoClient = KintoClient(httpClient, baseUrl, bucketName, collectionName)

        Mockito.doThrow(IOException()).`when`(httpClient).fetch(any())

        kintoClient.fetch("test")
    }

    @Test(expected = ExperimentDownloadException::class)
    fun handlesFetchErrorResponse() {
        val httpClient = mock(Client::class.java)
        val kintoClient = KintoClient(httpClient, baseUrl, bucketName, collectionName)

        Mockito.doReturn(Response("", 404, MutableHeaders(), Response.Body.empty())).`when`(httpClient).fetch(any())
        kintoClient.fetch("test")
    }
}