/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.fetch.okhttp

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.concept.fetch.Client
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.tooling.fetch.tests.FetchTestCases
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class OkHttpFetchTestCases : FetchTestCases() {

    override fun createNewClient(): Client = OkHttpClient(okhttp3.OkHttpClient(), testContext)

    // Inherits test methods from generic test suite base class

    @Test
    fun `Client instance`() {
        // We need at least one test case defined here so that this is recognized as test class.
        assertTrue(createNewClient() is OkHttpClient)
    }
}
