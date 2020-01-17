/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.push

import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.runBlockingTest
import mozilla.appservices.push.CommunicationError
import mozilla.appservices.push.CommunicationServerError
import mozilla.appservices.push.CryptoError
import mozilla.appservices.push.KeyInfo
import mozilla.appservices.push.RecordNotFoundError
import mozilla.appservices.push.SubscriptionInfo
import mozilla.appservices.push.SubscriptionResponse
import org.junit.Assert.assertEquals
import org.junit.Test

@ExperimentalCoroutinesApi
class AutoPushFeatureKtTest {
    @Test
    fun `transform response to PushSubscription`() {
        val response = SubscriptionResponse(
            "992a0f0542383f1ea5ef51b7cf4ae6c4",
            SubscriptionInfo("https://mozilla.com", KeyInfo("123", "456"))
        )
        val sub = response.toPushSubscription()

        assertEquals(PushType.Services, sub.type)
        assertEquals(response.subscriptionInfo.endpoint, sub.endpoint)
        assertEquals(response.subscriptionInfo.keys.auth, sub.authKey)
        assertEquals(response.subscriptionInfo.keys.p256dh, sub.publicKey)
    }

    @Test
    fun `asserts PushConfig's default values`() {
        val config = PushConfig("sample-browser")
        assertEquals("sample-browser", config.senderId)
        assertEquals("updates.push.services.mozilla.com", config.serverHost)
        assertEquals(Protocol.HTTPS, config.protocol)
        assertEquals(ServiceType.FCM, config.serviceType)

        val config2 = PushConfig("sample-browser", "push.test.mozilla.com", Protocol.HTTP, ServiceType.ADM)
        assertEquals("sample-browser", config2.senderId)
        assertEquals("push.test.mozilla.com", config2.serverHost)
        assertEquals(Protocol.HTTP, config2.protocol)
        assertEquals(ServiceType.ADM, config2.serviceType)
    }

    @Test(expected = CryptoError::class)
    fun `launchAndTry throws on unrecoverable Rust exceptions`() = runBlockingTest {
        CoroutineScope(coroutineContext).launchAndTry({ throw CryptoError("unit test") }, { assert(false) })
    }

    @Test
    fun `launchAndTry should NOT throw on recoverable Rust exceptions`() = runBlockingTest {
        CoroutineScope(coroutineContext).launchAndTry(
            { throw CommunicationServerError("should not fail test") },
            { assert(true) }
        )

        CoroutineScope(coroutineContext).launchAndTry(
            { throw CommunicationError("should not fail test") },
            { assert(true) }
        )

        CoroutineScope(coroutineContext).launchAndTry(
            { throw RecordNotFoundError("should not fail test") },
            { assert(true) }
        )
    }
}
