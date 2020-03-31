/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.fetch.httpurlconnection

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.concept.fetch.Client
import mozilla.components.concept.fetch.Request
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import java.net.HttpURLConnection
import java.net.URL

@RunWith(AndroidJUnit4::class)
class HttpUrlConnectionFetchTestCases : mozilla.components.tooling.fetch.tests.FetchTestCases() {
    override fun createNewClient(): Client = HttpURLConnectionClient()

    // Inherits test methods from generic test suite base class

    @Test
    fun `Client instance`() {
        // We need at least one test case defined here so that this is recognized as test class.
        assertTrue(createNewClient() is HttpURLConnectionClient)
    }

    @Test
    override fun get200WithCacheControl() {
        // We can't run the base fetch test case because HttpResponseCache
        // doesn't work in a unit test. So we test that we set the
        // flag correctly instead.
        val connection = (URL("https://mozilla.org").openConnection() as HttpURLConnection)
        assertTrue(connection.useCaches)

        connection.setupWith((Request("https://mozilla.org", useCaches = false)))
        assertFalse(connection.useCaches)
    }
}
