/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.gecko.cookiebanners

import junit.framework.TestCase.assertEquals
import junit.framework.TestCase.assertFalse
import junit.framework.TestCase.assertNull
import junit.framework.TestCase.assertTrue
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.runTest
import mozilla.components.concept.engine.EngineSession.CookieBannerHandlingMode.DISABLED
import mozilla.components.concept.engine.EngineSession.CookieBannerHandlingMode.REJECT_OR_ACCEPT_ALL
import mozilla.components.support.test.mock
import mozilla.components.support.test.whenever
import org.junit.Before
import org.junit.Test
import org.mockito.Mockito
import org.mockito.Mockito.doNothing
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify
import org.mozilla.geckoview.GeckoRuntime
import org.mozilla.geckoview.StorageController

@ExperimentalCoroutinesApi
class GeckoCookieBannersStorageTest {
    private lateinit var runtime: GeckoRuntime
    private lateinit var geckoStorage: GeckoCookieBannersStorage
    private lateinit var storageController: StorageController
    private lateinit var reportSiteDomainsRepository: ReportSiteDomainsRepository

    @Before
    fun setup() {
        storageController = mock()
        runtime = mock()
        reportSiteDomainsRepository = mock()

        whenever(runtime.storageController).thenReturn(storageController)

        geckoStorage = spy(GeckoCookieBannersStorage(runtime, reportSiteDomainsRepository))
    }

    @Test
    fun `GIVEN a cookie banner mode WHEN adding an exception THEN add an exception for the given uri and browsing mode`() =
        runTest {
            val uri = "https://www.mozilla.org"

            doNothing().`when`(geckoStorage)
                .setGeckoException(uri = uri, mode = DISABLED, privateBrowsing = false)

            geckoStorage.addException(uri = uri, privateBrowsing = false)

            verify(geckoStorage).setGeckoException(uri, DISABLED, false)
        }

    @Test
    fun `GIVEN uri and browsing mode WHEN removing an exception THEN remove the exception`() =
        runTest {
            val uri = "https://www.mozilla.org"

            doNothing().`when`(geckoStorage).removeGeckoException(uri, false)

            geckoStorage.removeException(uri = uri, privateBrowsing = false)

            verify(geckoStorage).removeGeckoException(uri, false)
        }

    @Test
    fun `GIVEN uri and browsing mode WHEN querying an exception THEN return the matching exception`() =
        runTest {
            val uri = "https://www.mozilla.org"

            doReturn(REJECT_OR_ACCEPT_ALL).`when`(geckoStorage)
                .queryExceptionInGecko(uri = uri, privateBrowsing = false)

            val result = geckoStorage.findExceptionFor(uri = uri, privateBrowsing = false)
            assertEquals(REJECT_OR_ACCEPT_ALL, result)
        }

    @Test
    fun `GIVEN error WHEN querying an exception THEN return null`() =
        runTest {
            val uri = "https://www.mozilla.org"

            doReturn(null).`when`(geckoStorage)
                .queryExceptionInGecko(uri = uri, privateBrowsing = false)

            val result = geckoStorage.findExceptionFor(uri = uri, privateBrowsing = false)
            assertNull(result)
        }

    @Test
    fun `GIVEN uri and browsing mode WHEN checking for an exception THEN indicate if it has exceptions`() =
        runTest {
            val uri = "https://www.mozilla.org"

            doReturn(REJECT_OR_ACCEPT_ALL).`when`(geckoStorage)
                .queryExceptionInGecko(uri = uri, privateBrowsing = false)

            var result = geckoStorage.hasException(uri = uri, privateBrowsing = false)

            assertFalse(result!!)

            Mockito.reset(geckoStorage)

            doReturn(DISABLED).`when`(geckoStorage)
                .queryExceptionInGecko(uri = uri, privateBrowsing = false)

            result = geckoStorage.hasException(uri = uri, privateBrowsing = false)

            assertTrue(result!!)
        }

    @Test
    fun `GIVEN an error WHEN checking for an exception THEN indicate if that an error happened`() =
        runTest {
            val uri = "https://www.mozilla.org"

            doReturn(null).`when`(geckoStorage)
                .queryExceptionInGecko(uri = uri, privateBrowsing = false)

            val result = geckoStorage.hasException(uri = uri, privateBrowsing = false)

            assertNull(result)
        }

    @Test
    fun `GIVEN a cookie banner mode WHEN adding a persistent exception in private mode THEN add a persistent exception for the given uri in private browsing mode`() =
        runTest {
            val uri = "https://www.mozilla.org"

            doNothing().`when`(geckoStorage)
                .setPersistentPrivateGeckoException(uri = uri, mode = DISABLED)

            geckoStorage.addPersistentExceptionInPrivateMode(uri = uri)

            verify(geckoStorage).setPersistentPrivateGeckoException(uri, DISABLED)
        }

    @Test
    fun `GIVEN site domain url WHEN checking if site domain is reported THEN the report site domain repository gets called`() =
        runTest {
            val reportSiteDomainUrl = "mozilla.org"

            geckoStorage.isSiteDomainReported(reportSiteDomainUrl)

            verify(reportSiteDomainsRepository).isSiteDomainReported(reportSiteDomainUrl)
        }

    @Test
    fun `GIVEN site domain url  WHEN saving a site domain THEN the save method from repository should get called`() =
        runTest {
            val reportSiteDomainUrl = "mozilla.org"

            geckoStorage.saveSiteDomain(reportSiteDomainUrl)

            verify(reportSiteDomainsRepository).saveSiteDomain(reportSiteDomainUrl)
        }
}
