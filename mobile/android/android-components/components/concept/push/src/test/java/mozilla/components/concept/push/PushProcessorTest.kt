/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.push

import mozilla.components.support.test.mock
import org.junit.Assert.assertNotNull
import org.junit.Before
import org.junit.Test

class PushProcessorTest {

    @Before
    fun setup() {
        PushProcessor.reset()
    }

    @Test
    fun install() {
        val processor: PushProcessor = mock()

        PushProcessor.install(processor)

        assertNotNull(PushProcessor.requireInstance)
    }

    @Test(expected = IllegalStateException::class)
    fun `requireInstance throws if install not called first`() {
        PushProcessor.requireInstance
    }

    @Test
    fun init() {
        val push = TestPushProcessor()

        PushProcessor.install(push)

        assertNotNull(PushProcessor.requireInstance)
    }

    class TestPushProcessor : PushProcessor {
        override fun initialize() {}

        override fun shutdown() {}

        override fun onNewToken(newToken: String) {}

        override fun onMessageReceived(message: EncryptedPushMessage) {}

        override fun onError(error: PushError) {}

        override fun renewRegistration() {}
    }
}
