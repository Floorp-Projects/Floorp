/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.push

import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.runTest
import mozilla.appservices.push.DispatchInfo
import mozilla.appservices.push.KeyInfo
import mozilla.appservices.push.PushManager
import mozilla.appservices.push.SubscriptionInfo
import mozilla.appservices.push.SubscriptionResponse
import mozilla.components.support.test.any
import mozilla.components.support.test.eq
import mozilla.components.support.test.mock
import mozilla.components.support.test.nullable
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Ignore
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers
import org.mockito.Mockito.anyString
import org.mockito.Mockito.never
import org.mockito.Mockito.verify
import org.mockito.Mockito.`when`

@ExperimentalCoroutinesApi // for runTest
@RunWith(AndroidJUnit4::class)
class RustPushConnectionTest {

    @Ignore("Requires push-forUnitTests; seems unnecessary to introduce it for this one test.")
    @Test
    fun `new token initializes API`() = runTest {
        val connection = createConnection()

        assertNull(connection.api)

        connection.updateToken("token")

        assertNotNull(connection.api)
    }

    @Test
    fun `new token calls update if API is already initialized`() = runTest {
        val connection = createConnection()
        val api: PushManager = mock()
        connection.api = api

        connection.updateToken("123")

        verify(api, never()).subscribe(any(), any(), any())
        verify(api).update(anyString())
    }

    @Test(expected = IllegalStateException::class)
    fun `subscribe throws if API is not initialized first`() = runTest {
        val connection = createConnection()

        connection.subscribe("123")
    }

    @Test
    fun `subscribe calls Rust API`() = runTest {
        val connection = createConnection()
        val api: PushManager = mock()
        val response = SubscriptionResponse(
            channelId = "1234",
            subscriptionInfo = SubscriptionInfo(
                endpoint = "https://foo",
                keys = KeyInfo(
                    auth = "auth",
                    p256dh = "p256dh",
                ),
            ),
        )

        connection.api = api

        `when`(api.subscribe(anyString(), anyString(), nullable())).thenReturn(response)

        val sub = connection.subscribe("123")

        assertEquals("123", sub.scope)
        assertEquals("auth", sub.authKey)
        assertEquals("p256dh", sub.publicKey)
        assertEquals("https://foo", sub.endpoint)

        verify(api).subscribe(anyString(), anyString(), nullable())
    }

    @Test(expected = IllegalStateException::class)
    fun `unsubscribe throws if API is not initialized first`() = runTest {
        val connection = createConnection()

        connection.unsubscribe("123")
    }

    @Test
    fun `unsubscribe calls Rust API`() = runTest {
        val connection = createConnection()
        val api: PushManager = mock()
        connection.api = api

        connection.unsubscribe("123")

        verify(api).unsubscribe(anyString())
    }

    @Test(expected = IllegalStateException::class)
    fun `unsubscribeAll throws if API is not initialized first`() = runTest {
        val connection = createConnection()

        connection.unsubscribeAll()
    }

    @Test
    fun `unsubscribeAll calls Rust API`() = runTest {
        val connection = createConnection()
        val api: PushManager = mock()
        connection.api = api

        connection.unsubscribeAll()

        verify(api).unsubscribeAll()
    }

    @Test
    fun `containsSubscription returns true if a subscription exists`() = runTest {
        val connection = createConnection()
        val api: PushManager = mock()
        connection.api = api

        `when`(api.dispatchInfoForChid(ArgumentMatchers.anyString()))
            .thenReturn(mock())
            .thenReturn(null)

        assertTrue(connection.containsSubscription("validSubscription"))
        assertFalse(connection.containsSubscription("invalidSubscription"))
    }

    @Test(expected = IllegalStateException::class)
    fun `verifyConnection throws if API is not initialized first`() = runTest {
        val connection = createConnection()

        connection.verifyConnection()
    }

    @Test
    fun `verifyConnection calls Rust API`() = runTest {
        val connection = createConnection()
        val api: PushManager = mock()
        connection.api = api

        connection.verifyConnection()

        verify(api).verifyConnection()
    }

    @Test(expected = IllegalStateException::class)
    fun `decrypt throws if API is not initialized first`() = runTest {
        val connection = createConnection()

        connection.decryptMessage("123", "plain text")
    }

    @Test
    fun `decrypt calls Rust API`() = runTest {
        val connection = createConnection()
        val api: PushManager = mock()
        val dispatchInfo: DispatchInfo = mock()
        connection.api = api

        connection.decryptMessage("123", "body")

        verify(api, never()).decrypt(anyString(), anyString(), eq(""), eq(""), eq(""))

        `when`(api.dispatchInfoForChid(anyString())).thenReturn(dispatchInfo)
        `when`(dispatchInfo.scope).thenReturn("test")

        connection.decryptMessage("123", "body")

        verify(api).decrypt(anyString(), anyString(), eq(""), eq(""), eq(""))

        connection.decryptMessage("123", "body", "enc", "salt", "key")

        verify(api).decrypt(anyString(), anyString(), eq("enc"), eq("salt"), eq("key"))
    }

    @Test
    fun `empty body decrypts nothing`() = runTest {
        val connection = createConnection()
        val api: PushManager = mock()
        val dispatchInfo: DispatchInfo = mock()
        connection.api = api

        connection.decryptMessage("123", null)

        verify(api, never()).decrypt(anyString(), anyString(), eq(""), eq(""), eq(""))

        `when`(api.dispatchInfoForChid(anyString())).thenReturn(dispatchInfo)
        `when`(dispatchInfo.scope).thenReturn("test")

        val (scope, message) = connection.decryptMessage("123", null)!!
        assertEquals("test", scope)
        assertNull(message)

        verify(api, never()).decrypt(anyString(), nullable(), eq(""), eq(""), eq(""))
    }

    @Test(expected = IllegalStateException::class)
    fun `close throws if API is not initialized first`() = runTest {
        val connection = createConnection()

        connection.close()
    }

    @Test
    fun `close calls Rust API`() = runTest {
        val connection = createConnection()
        val api: PushManager = mock()
        connection.api = api

        connection.close()

        verify(api).close()
    }

    @Test
    fun `initialized is true when api is not null`() = runTest {
        val connection = createConnection()

        assertFalse(connection.isInitialized())

        connection.api = mock()

        assertTrue(connection.isInitialized())
    }

    private fun createConnection() = RustPushConnection(
        testContext,
        "push-test",
        "push.mozilla.com",
        Protocol.HTTPS,
        ServiceType.FCM,
    )
}
