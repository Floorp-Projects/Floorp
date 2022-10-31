/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.push

import mozilla.appservices.push.BridgeType
import mozilla.appservices.push.KeyInfo
import mozilla.appservices.push.SubscriptionInfo
import mozilla.appservices.push.SubscriptionResponse
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import mozilla.appservices.push.PushSubscriptionChanged as SubscriptionChanged

class ConnectionKtTest {
    @Test
    fun `transform response to PushSubscription`() {
        val response = SubscriptionResponse(
            "992a0f0542383f1ea5ef51b7cf4ae6c4",
            SubscriptionInfo("https://mozilla.com", KeyInfo("123", "456")),
        )
        val sub = response.toPushSubscription("scope")

        assertEquals(response.subscriptionInfo.endpoint, sub.endpoint)
        assertEquals(response.subscriptionInfo.keys.auth, sub.authKey)
        assertEquals(response.subscriptionInfo.keys.p256dh, sub.publicKey)
        assertEquals("scope", sub.scope)
        assertNull(sub.appServerKey)

        val sub2 = response.toPushSubscription("scope", "key")

        assertEquals(response.subscriptionInfo.endpoint, sub.endpoint)
        assertEquals(response.subscriptionInfo.keys.auth, sub.authKey)
        assertEquals(response.subscriptionInfo.keys.p256dh, sub.publicKey)
        assertEquals("scope", sub2.scope)
        assertEquals("key", sub2.appServerKey)
    }

    @Test
    fun `ServiceType to BridgeType`() {
        assertEquals(BridgeType.FCM, ServiceType.FCM.toBridgeType())
    }

    @Test
    fun `Protocol to string`() {
        assertEquals("http", Protocol.HTTP.asString())
        assertEquals("https", Protocol.HTTPS.asString())
    }

    @Test
    fun `transform response to PushSubscriptionChanged`() {
        val response = SubscriptionChanged(
            "992a0f0542383f1ea5ef51b7cf4ae6c4",
            "scope",
        )
        val sub = response.toPushSubscriptionChanged()
        assertEquals(response.channelId, sub.channelId)
        assertEquals(response.scope, sub.scope)
    }

    @Test
    fun `decrypted message equals`() {
        val message1 = DecryptedMessage("test", "message1".toByteArray())
        val message2 = DecryptedMessage("test", "message2".toByteArray())

        assertTrue(message1 != message2)

        val message3 = DecryptedMessage("test", "message".toByteArray())
        val message4 = DecryptedMessage("test", null)

        assertTrue(message3 != message4)

        val message5 = DecryptedMessage("test", null)
        val message6 = DecryptedMessage("test", "message".toByteArray())

        assertTrue(message5 != message6)

        val message7 = DecryptedMessage("test", "message".toByteArray())
        val message8 = DecryptedMessage("test", "message".toByteArray())

        assertTrue(message7 == message8)
    }

    @Test
    fun `decrypted message hashcode`() {
        val message1 = DecryptedMessage("test", "message1".toByteArray())
        val message2 = DecryptedMessage("test", "message2".toByteArray())

        assertTrue(message1.hashCode() != message2.hashCode())

        val message3 = DecryptedMessage("test", "message".toByteArray())
        val message4 = DecryptedMessage("test", "message".toByteArray())

        assertTrue(message3.hashCode() == message4.hashCode())
    }
}
