/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.metrics

import android.util.Base64
import com.google.android.gms.ads.identifier.AdvertisingIdClient
import com.google.android.gms.common.GooglePlayServicesNotAvailableException
import com.google.android.gms.common.GooglePlayServicesRepairableException
import io.mockk.Runs
import io.mockk.every
import io.mockk.just
import io.mockk.mockk
import io.mockk.mockkStatic
import io.mockk.slot
import io.mockk.spyk
import io.mockk.verify
import kotlinx.coroutines.runBlocking
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Ignore
import org.junit.Test
import org.mockito.ArgumentMatchers.any
import org.mockito.ArgumentMatchers.anyString
import java.io.IOException

internal class ActivationPingTest {
    @Ignore("This test has side-effects that cause it to fail other unrelated tests.")
    @Test
    fun `getAdvertisingID() returns null if the API throws`() {
        mockkStatic(AdvertisingIdClient::class)

        val exceptions = listOf(
            GooglePlayServicesNotAvailableException(1),
            GooglePlayServicesRepairableException(0, anyString(), any()),
            IllegalStateException(),
            IOException()
        )

        val ap = ActivationPing(mockk())
        exceptions.forEach {
            every {
                AdvertisingIdClient.getAdvertisingIdInfo(any())
            } throws it

            assertNull(ap.getAdvertisingID())
        }
    }

    @Test
    fun `getAdvertisingID() returns null if the API returns null info`() {
        mockkStatic(AdvertisingIdClient::class)
        every { AdvertisingIdClient.getAdvertisingIdInfo(any()) } returns null

        val ap = ActivationPing(mockk())
        assertNull(ap.getAdvertisingID())
    }

    @Test
    fun `getAdvertisingID() returns a valid string if the API returns a valid ID`() {
        val testId = "test-value-id"

        mockkStatic(AdvertisingIdClient::class)
        every {
            AdvertisingIdClient.getAdvertisingIdInfo(any())
        } returns AdvertisingIdClient.Info(testId, false)

        val ap = ActivationPing(mockk())
        assertEquals(testId, ap.getAdvertisingID())
    }

    @Test
    fun `getHashedIdentifier() returns an hashed identifier`() {
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
        val mockAp = spyk(ActivationPing(mockk()))
        every { mockAp.getAdvertisingID() } returns testId
        every { mockAp.getHashingSalt() } returns testPackageName
        runBlocking {
            assertEquals(mockedHexReturn, mockAp.getHashedIdentifier())
        }

        // Check that the digest of the identifier matches with what we expect.
        // Please note that in the real world, Base64.encodeToString would encode
        // this to something much shorter, which we'd send with the ping.
        val expectedDigestBytes =
            "[52, -79, -84, 79, 101, 22, -82, -44, -44, -14, 21, 15, 48, 88, -94, -74, -8, 25, -72, -120, -37, 108, 47, 16, 2, -37, 126, 41, 102, -92, 103, 24]"
        assertEquals(expectedDigestBytes, shaDigest.captured.contentToString())
    }

    @Test
    fun `checkAndSend() triggers the ping if it wasn't marked as triggered`() {
        val mockAp = spyk(ActivationPing(mockk()), recordPrivateCalls = true)
        every { mockAp.wasAlreadyTriggered() } returns false
        every { mockAp.markAsTriggered() } just Runs

        mockAp.checkAndSend()

        verify(exactly = 1) { mockAp.triggerPing() }
        // Marking the ping as triggered happens in a co-routine off the main thread,
        // so wait a bit for it.
        verify(timeout = 5000, exactly = 1) { mockAp.markAsTriggered() }
    }

    @Test
    fun `checkAndSend() doesn't trigger the ping again if it was marked as triggered`() {
        val mockAp = spyk(ActivationPing(mockk()), recordPrivateCalls = true)
        every { mockAp.wasAlreadyTriggered() } returns true

        mockAp.checkAndSend()

        verify(exactly = 0) { mockAp.triggerPing() }
    }
}
