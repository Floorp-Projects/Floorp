/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.customtabs.verify

import android.content.pm.PackageManager
import androidx.browser.customtabs.CustomTabsService.RELATION_HANDLE_ALL_URLS
import androidx.browser.customtabs.CustomTabsService.RELATION_USE_AS_ORIGIN
import androidx.core.net.toUri
import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.runBlocking
import mozilla.components.concept.fetch.Client
import mozilla.components.concept.fetch.Request
import mozilla.components.concept.fetch.Response
import mozilla.components.support.test.any
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mock
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify
import org.mockito.MockitoAnnotations.initMocks
import java.util.concurrent.TimeUnit

@RunWith(AndroidJUnit4::class)
@ExperimentalCoroutinesApi
class OriginVerifierTest {

    private val apiKey = "XXXXXXXXX"
    @Mock private lateinit var client: Client
    @Mock private lateinit var packageManager: PackageManager
    @Mock private lateinit var response: Response
    @Mock private lateinit var body: Response.Body
    private lateinit var handleAllUrlsVerifier: OriginVerifier
    private lateinit var useAsOriginVerifier: OriginVerifier

    @Suppress("Deprecation")
    @Before
    fun setup() {
        initMocks(this)
        handleAllUrlsVerifier =
            spy(OriginVerifier(testContext.packageName, RELATION_HANDLE_ALL_URLS, packageManager, client, apiKey))
        useAsOriginVerifier =
            spy(OriginVerifier(testContext.packageName, RELATION_USE_AS_ORIGIN, packageManager, client, apiKey))

        doReturn(response).`when`(client).fetch(any())
        doReturn(body).`when`(response).body
        doReturn(200).`when`(response).status
        doReturn("AA:BB:CC:10:20:30:01:02").`when`(handleAllUrlsVerifier).signatureFingerprint
        doReturn("AA:BB:CC:10:20:30:01:02").`when`(useAsOriginVerifier).signatureFingerprint
        doReturn("{\"linked\":true}").`when`(body).string()
    }

    @Test
    fun getCertificateSHA256FingerprintForPackage() {
        assertEquals(
            "AA:BB:CC:10:20:30:01:02",
            OriginVerifier.byteArrayToHexString(byteArrayOf(0xaa.toByte(), 0xbb.toByte(), 0xcc.toByte(), 0x10, 0x20, 0x30, 0x01, 0x02))
        )
    }

    @Test
    fun `only HTTPS allowed`() = runBlocking {
        assertFalse(handleAllUrlsVerifier.verifyOrigin("LOL".toUri()))
        assertFalse(handleAllUrlsVerifier.verifyOrigin("http://www.android.com".toUri()))
    }

    @Test
    fun verifyOrigin() = runBlocking {
        assertTrue(useAsOriginVerifier.verifyOrigin("https://www.example.com".toUri()))

        verify(client).fetch(
            Request(
                DigitalAssetLinksHandler.getUrlForCheckingRelationship(
                    "https://www.example.com",
                    testContext.packageName,
                    "AA:BB:CC:10:20:30:01:02",
                    "delegate_permission/common.use_as_origin",
                    apiKey
                ),
                connectTimeout = 3L to TimeUnit.SECONDS,
                readTimeout = 3L to TimeUnit.SECONDS
            )
        )

        Unit
    }
}
