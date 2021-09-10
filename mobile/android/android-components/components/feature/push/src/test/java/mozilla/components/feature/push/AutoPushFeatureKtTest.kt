/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.push

import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.launch
import kotlinx.coroutines.plus
import kotlinx.coroutines.test.runBlockingTest
import mozilla.appservices.push.PushException.CommunicationException
import mozilla.appservices.push.PushException.CommunicationServerException
import mozilla.appservices.push.PushException.CryptoException
import mozilla.appservices.push.PushException.GeneralException
import mozilla.appservices.push.PushException.MissingRegistrationTokenException
import mozilla.components.concept.push.PushError
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

@ExperimentalCoroutinesApi
class AutoPushFeatureKtTest {

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

    @Test
    fun `exception handler handles exceptions`() = runBlockingTest {
        var invoked = false
        val scope = CoroutineScope(coroutineContext) + exceptionHandler { invoked = true }

        scope.launch { throw PushError.MalformedMessage("test") }
        assertFalse(invoked)

        scope.launch { throw GeneralException("test") }
        assertFalse(invoked)

        scope.launch { throw CryptoException("test") }
        assertFalse(invoked)

        scope.launch { throw CommunicationException("test") }
        assertFalse(invoked)

        scope.launch { throw CommunicationServerException("test") }
        assertFalse(invoked)

        // An exception where we should invoke our callback.
        scope.launch { throw MissingRegistrationTokenException("") }
        assertTrue(invoked)
    }
}
