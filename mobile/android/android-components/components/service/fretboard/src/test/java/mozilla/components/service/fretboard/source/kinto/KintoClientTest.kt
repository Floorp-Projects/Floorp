/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.fretboard.source.kinto

import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.mock
import org.mockito.Mockito.verify
import org.robolectric.RobolectricTestRunner
import java.net.URL

@RunWith(RobolectricTestRunner::class)
class KintoClientTest {
    private val baseUrl = "http://example.test"
    private val bucketName = "fretboard"
    private val collectionName = "experiments"

    @Test
    fun testGet() {
        val httpClient = mock(HttpClient::class.java)
        val kintoClient = KintoClient(httpClient, baseUrl, bucketName, collectionName)
        kintoClient.get()
        verify(httpClient).get(URL("http://example.test/buckets/fretboard/collections/experiments/records"))
    }

    @Test
    fun testDiff() {
        val httpClient = mock(HttpClient::class.java)
        val kintoClient = KintoClient(httpClient, baseUrl, bucketName, collectionName)
        kintoClient.diff(1527179995)
        verify(httpClient).get(URL("http://example.test/buckets/fretboard/collections/experiments/records?_since=1527179995"))
    }
}