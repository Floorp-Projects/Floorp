/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.metrics

import android.content.Context
import android.util.Base64
import com.google.android.gms.ads.identifier.AdvertisingIdClient
import com.google.android.gms.common.GooglePlayServicesNotAvailableException
import com.google.android.gms.common.GooglePlayServicesRepairableException
import io.mockk.every
import io.mockk.mockk
import io.mockk.mockkObject
import io.mockk.mockkStatic
import io.mockk.slot
import io.mockk.unmockkStatic
import kotlinx.coroutines.test.runTest
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test
import java.io.IOException

class MetricsUtilsTest {

    private val context: Context = mockk(relaxed = true)

    @Test
    fun `getAdvertisingID() returns null if the API throws`() {
        mockkStatic("com.google.android.gms.ads.identifier.AdvertisingIdClient")

        val exceptions = listOf(
            GooglePlayServicesNotAvailableException(1),
            GooglePlayServicesRepairableException(0, "", mockk()),
            IllegalStateException(),
            IOException(),
        )

        exceptions.forEach {
            every {
                AdvertisingIdClient.getAdvertisingIdInfo(any())
            } throws it

            assertNull(MetricsUtils.getAdvertisingID(context))
        }

        unmockkStatic("com.google.android.gms.ads.identifier.AdvertisingIdClient")
    }

    @Test
    fun `getAdvertisingID() returns null if the API returns null info`() {
        mockkStatic(AdvertisingIdClient::class)
        every { AdvertisingIdClient.getAdvertisingIdInfo(any()) } returns null

        assertNull(MetricsUtils.getAdvertisingID(context))
    }

    @Test
    fun `getAdvertisingID() returns a valid string if the API returns a valid ID`() {
        val testId = "test-value-id"

        mockkStatic(AdvertisingIdClient::class)
        every {
            AdvertisingIdClient.getAdvertisingIdInfo(any())
        } returns AdvertisingIdClient.Info(testId, false)

        assertEquals(testId, MetricsUtils.getAdvertisingID(context))
    }

    @Test
    fun `getHashedIdentifier() returns a hashed identifier`() = runTest {
        val testId = "test-value-id"
        val testPackageName = "org.mozilla-test.fenix"
        val mockedHexReturn = "mocked-HEX"

        // Mock the Base64 to record the byte array that is passed in,
        // which is the actual digest. We can't simply test the return value
        // of |getHashedIdentifier| as these Android tests require us to mock
        // Android-specific APIs.
        mockkStatic(Base64::class)
        val shaDigest = slot<ByteArray>()
        every {
            Base64.encodeToString(capture(shaDigest), any())
        } returns mockedHexReturn

        // Get the hash identifier.
        mockkObject(MetricsUtils)
        every { MetricsUtils.getAdvertisingID(context) } returns testId
        every { MetricsUtils.getHashingSalt() } returns testPackageName
        assertEquals(mockedHexReturn, MetricsUtils.getHashedIdentifier(context))

        // Check that the digest of the identifier matches with what we expect.
        // Please note that in the real world, Base64.encodeToString would encode
        // this to something much shorter, which we'd send with the ping.
        val expectedDigestBytes =
            "[52, -79, -84, 79, 101, 22, -82, -44, -44, -14, 21, 15, 48, 88, -94, -74, -8, 25, -72, -120, -37, 108, 47, 16, 2, -37, 126, 41, 102, -92, 103, 24]"
        assertEquals(expectedDigestBytes, shaDigest.captured.contentToString())
    }

    companion object {
        const val ENGINE_SOURCE_IDENTIFIER = "google-2018"
    }
}
