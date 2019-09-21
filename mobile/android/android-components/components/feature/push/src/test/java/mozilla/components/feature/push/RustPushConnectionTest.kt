/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.push

import kotlinx.coroutines.runBlocking
import mozilla.appservices.push.PushAPI
import mozilla.components.support.test.eq
import mozilla.components.support.test.mock
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Ignore
import org.junit.Test
import org.mockito.Mockito.anyString
import org.mockito.Mockito.verify

class RustPushConnectionTest {

    @Ignore("Requires push-forUnitTests; seems unnecessary to introduce it for this one test.")
    @Test
    fun `new token initializes API`() {
        val connection = createConnection()

        assertNull(connection.api)

        runBlocking {
            connection.updateToken("token")
        }

        assertNotNull(connection.api)
    }

    @Test
    fun `new token calls update if API is already initialized`() {
        val connection = createConnection()
        val api: PushAPI = mock()
        connection.api = api

        runBlocking {
            connection.updateToken("123")
        }

        verify(api).update(anyString())
    }

    @Test(expected = IllegalStateException::class)
    fun `subscribe throws if API is not initialized first`() {
        val connection = createConnection()

        runBlocking {
            connection.subscribe("123", "")
        }
    }

    @Test
    fun `subscribe calls Rust API`() {
        val connection = createConnection()
        val api: PushAPI = mock()
        connection.api = api

        runBlocking {
            connection.subscribe("123", "")
        }

        verify(api).subscribe(anyString(), anyString())
    }

    @Test(expected = IllegalStateException::class)
    fun `unsubscribe throws if API is not initialized first`() {
        val connection = createConnection()

        runBlocking {
            connection.unsubscribe("123")
        }
    }

    @Test
    fun `unsubscribe calls Rust API`() {
        val connection = createConnection()
        val api: PushAPI = mock()
        connection.api = api

        runBlocking {
            connection.unsubscribe("123")
        }

        verify(api).unsubscribe(anyString())
    }

    @Test(expected = IllegalStateException::class)
    fun `unsubscribeAll throws if API is not initialized first`() {
        val connection = createConnection()

        runBlocking {
            connection.unsubscribeAll()
        }
    }

    @Test
    fun `unsubscribeAll calls Rust API`() {
        val connection = createConnection()
        val api: PushAPI = mock()
        connection.api = api

        runBlocking {
            connection.unsubscribeAll()
        }

        verify(api).unsubscribe("")
    }

    @Test(expected = IllegalStateException::class)
    fun `verifyConnection throws if API is not initialized first`() {
        val connection = createConnection()

        runBlocking {
            connection.verifyConnection()
        }
    }

    @Test
    fun `verifyConnection calls Rust API`() {
        val connection = createConnection()
        val api: PushAPI = mock()
        connection.api = api

        runBlocking {
            connection.verifyConnection()
        }

        verify(api).verifyConnection()
    }

    @Test(expected = IllegalStateException::class)
    fun `decrypt throws if API is not initialized first`() {
        val connection = createConnection()

        runBlocking {
            connection.decrypt("123", "plain text")
        }
    }

    @Test
    fun `decrypt calls Rust API`() {
        val connection = createConnection()
        val api: PushAPI = mock()
        connection.api = api

        runBlocking {
            connection.decrypt("123", "body")
        }

        verify(api).decrypt(anyString(), anyString(), eq(""), eq(""), eq(""))

        runBlocking {
            connection.decrypt("123", "body", "enc", "salt", "key")
        }

        verify(api).decrypt(anyString(), anyString(), eq("enc"), eq("salt"), eq("key"))
    }

    @Test(expected = IllegalStateException::class)
    fun `close throws if API is not initialized first`() {
        val connection = createConnection()

        runBlocking {
            connection.close()
        }
    }

    @Test
    fun `close calls Rust API`() {
        val connection = createConnection()
        val api: PushAPI = mock()
        connection.api = api

        runBlocking {
            connection.close()
        }

        verify(api).close()
    }

    @Test
    fun `initialized is true when api is not null`() {
        val connection = createConnection()

        assertFalse(connection.isInitialized())

        connection.api = mock()

        assertTrue(connection.isInitialized())
    }

    private fun createConnection() = RustPushConnection(
        "/sdcard/",
        "push-test",
        "push.mozilla.com",
        Protocol.HTTPS,
        ServiceType.FCM
    )
}
