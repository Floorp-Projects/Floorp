/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.gecko.fetch.geckoview

import androidx.test.annotation.UiThreadTest
import androidx.test.core.app.ApplicationProvider
import androidx.test.filters.MediumTest
import mozilla.components.browser.engine.gecko.fetch.GeckoViewFetchClient
import mozilla.components.concept.fetch.Client
import org.junit.Assert.assertTrue
import org.junit.Test

@MediumTest
class GeckoViewFetchTestCases : mozilla.components.tooling.fetch.tests.FetchTestCases() {
    override fun createNewClient(): Client = GeckoViewFetchClient(ApplicationProvider.getApplicationContext())

    @Test
    @UiThreadTest
    fun clientInstance() {
        assertTrue(createNewClient() is GeckoViewFetchClient)
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
    override fun put201FileUpload() {
        super.put201FileUpload()
    }

    @Test
    @UiThreadTest
    override fun get200WithCookiePolicy() {
        super.get200WithCookiePolicy()
    }

    @Test
    @UiThreadTest
    override fun get200WithContentTypeCharset() {
        super.get200WithContentTypeCharset()
    }

    @Test
    @UiThreadTest
    override fun get200WithCacheControl() {
        super.get200WithCacheControl()
    }

    @Test
    @UiThreadTest
    override fun getThrowsIOExceptionWhenHostNotReachable() {
        super.getThrowsIOExceptionWhenHostNotReachable()
    }

    @Test
    @UiThreadTest
    override fun getDataUri() {
        super.getDataUri()
    }
}
