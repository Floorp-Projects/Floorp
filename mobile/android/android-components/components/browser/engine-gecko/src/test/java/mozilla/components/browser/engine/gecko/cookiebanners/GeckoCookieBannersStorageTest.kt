/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.gecko.cookiebanners

import androidx.test.ext.junit.runners.AndroidJUnit4
import junit.framework.TestCase.assertEquals
import junit.framework.TestCase.assertFalse
import junit.framework.TestCase.assertTrue
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.runTest
import mozilla.components.concept.engine.EngineSession.CookieBannerHandlingMode.DISABLED
import mozilla.components.concept.engine.EngineSession.CookieBannerHandlingMode.REJECT_OR_ACCEPT_ALL
import mozilla.components.support.test.mock
import mozilla.components.support.test.whenever
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito
import org.mockito.Mockito.doNothing
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify
import org.mozilla.geckoview.GeckoRuntime
import org.mozilla.geckoview.StorageController

@ExperimentalCoroutinesApi
@RunWith(AndroidJUnit4::class)
class GeckoCookieBannersStorageTest {
    private lateinit var runtime: GeckoRuntime
    private lateinit var geckoStorage: GeckoCookieBannersStorage
    private lateinit var storageController: StorageController

    @Before
    fun setup() {
        storageController = mock()
        runtime = mock()

        whenever(runtime.storageController).thenReturn(storageController)

        geckoStorage = spy(GeckoCookieBannersStorage(runtime))
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
    fun `GIVEN uri and browsing mode WHEN checking for an exception THEN indicate if it has exceptions`() =
        runTest {
            val uri = "https://www.mozilla.org"

            doReturn(REJECT_OR_ACCEPT_ALL).`when`(geckoStorage)
                .queryExceptionInGecko(uri = uri, privateBrowsing = false)

            var result = geckoStorage.hasException(uri = uri, privateBrowsing = false)

            assertFalse(result)

            Mockito.reset(geckoStorage)

            doReturn(DISABLED).`when`(geckoStorage)
                .queryExceptionInGecko(uri = uri, privateBrowsing = false)

            result = geckoStorage.hasException(uri = uri, privateBrowsing = false)

            assertTrue(result)
        }
}
