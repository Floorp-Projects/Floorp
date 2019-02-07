/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.fetch.geckoview

import androidx.test.annotation.UiThreadTest
import androidx.test.core.app.ApplicationProvider
import androidx.test.filters.MediumTest
import mozilla.components.browser.engine.gecko.fetch.GeckoViewFetchClient
import mozilla.components.concept.fetch.Client
import org.junit.Assert.assertTrue
import org.junit.Ignore
import org.junit.Test

@MediumTest
@Suppress("TooManyFunctions")
class GeckoViewFetchTestCases : mozilla.components.tooling.fetch.tests.FetchTestCases() {
    override fun createNewClient(): Client = GeckoViewFetchClient(ApplicationProvider.getApplicationContext())

    @Test
    @UiThreadTest
    fun clientInstance() {
        assertTrue(createNewClient() is GeckoViewFetchClient)
    }

    @Test
    @UiThreadTest
    override fun get200WithDefaultHeaders() {
        super.get200WithDefaultHeaders()
    }

    @Test
    @UiThreadTest
    override fun get200WithGzippedBody() {
        super.get200WithGzippedBody()
    }

    @Test
    @UiThreadTest
    override fun get200OverridingDefaultHeaders() {
        super.get200OverridingDefaultHeaders()
    }

    @Test
    @UiThreadTest
    override fun get200WithUserAgent() {
        super.get200WithUserAgent()
    }

    @Test
    @UiThreadTest
    override fun get200WithDuplicatedCacheControlRequestHeaders() {
        super.get200WithDuplicatedCacheControlRequestHeaders()
    }

    @Test
    @UiThreadTest
    override fun get200WithDuplicatedCacheControlResponseHeaders() {
        super.get200WithDuplicatedCacheControlResponseHeaders()
    }

    @Test
    @UiThreadTest
    override fun get200WithHeaders() {
        super.get200WithHeaders()
    }

    @Test
    @UiThreadTest
    override fun get200WithReadTimeout() {
        super.get200WithReadTimeout()
    }

    @Test
    @UiThreadTest
    override fun get200WithStringBody() {
        super.get200WithStringBody()
    }

    @Test
    @UiThreadTest
    override fun get302FollowRedirects() {
        super.get302FollowRedirects()
    }

    @Test
    @UiThreadTest
    @Ignore("Blocked on: https://bugzilla.mozilla.org/show_bug.cgi?id=1526327")
    override fun get302FollowRedirectsDisabled() {
        super.get302FollowRedirectsDisabled()
    }

    @Test
    @UiThreadTest
    override fun get404WithBody() {
        super.get404WithBody()
    }

    @Test
    @UiThreadTest
    override fun post200WithBody() {
        super.post200WithBody()
    }

    @Test
    @UiThreadTest
    @Ignore("Blocked on: https://bugzilla.mozilla.org/show_bug.cgi?id=1526322")
    override fun put201FileUpload() {
        super.put201FileUpload()
    }
}
