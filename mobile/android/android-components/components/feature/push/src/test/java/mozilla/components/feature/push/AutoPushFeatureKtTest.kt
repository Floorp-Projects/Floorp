/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.push

import kotlinx.coroutines.DelicateCoroutinesApi
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.GlobalScope
import kotlinx.coroutines.launch
import kotlinx.coroutines.test.runTest
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
    }

    @OptIn(DelicateCoroutinesApi::class)
    @Test
    fun `exception handler handles exceptions`() = runTest {
        var invoked = false
        val handler = exceptionHandler { invoked = true }

        GlobalScope.launch(handler) { throw PushError.MalformedMessage("test") }.join()
        assertFalse(invoked)

        GlobalScope.launch(handler) { throw GeneralException("test") }.join()
        assertFalse(invoked)

        GlobalScope.launch(handler) { throw CryptoException("test") }.join()
        assertFalse(invoked)

        GlobalScope.launch(handler) { throw CommunicationException("test") }.join()
        assertFalse(invoked)

        GlobalScope.launch(handler) { throw CommunicationServerException("test") }.join()
        assertFalse(invoked)

        // An exception where we should invoke our callback.
        GlobalScope.launch(handler) { throw MissingRegistrationTokenException("") }.join()
        assertTrue(invoked)
    }
}
