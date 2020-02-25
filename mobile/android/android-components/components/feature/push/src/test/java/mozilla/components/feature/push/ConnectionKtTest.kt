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
import org.junit.Test

class ConnectionKtTest {
    @Test
    @Suppress("Deprecation")
    fun `transform response to PushSubscription`() {
        val response = SubscriptionResponse(
            "992a0f0542383f1ea5ef51b7cf4ae6c4",
            SubscriptionInfo("https://mozilla.com", KeyInfo("123", "456"))
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
        assertEquals(BridgeType.ADM, ServiceType.ADM.toBridgeType())
    }

    @Test
    fun `Protocol to string`() {
        assertEquals("http", Protocol.HTTP.asString())
        assertEquals("https", Protocol.HTTPS.asString())
    }
}
