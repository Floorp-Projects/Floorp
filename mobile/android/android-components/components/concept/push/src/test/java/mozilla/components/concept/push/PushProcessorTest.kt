/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 *  License, v. 2.0. If a copy of the MPL was not distributed with this
 *  file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

package mozilla.components.concept.push

import org.junit.Assert.assertNotNull
import org.junit.Test

class PushProcessorTest {

    @Test
    fun init() {
        val push = TestPushProcessor()

        push.initialize()

        assertNotNull(PushProcessor.requireInstance)
    }

    @Test(expected = IllegalStateException::class)
    fun `requireInstance throws exception if not initialized`() {
        PushProcessor.requireInstance
    }

    class TestPushProcessor : PushProcessor() {
        override fun start() {}

        override fun stop() {}

        override fun onNewToken(newToken: String) {}

        override fun onMessageReceived(message: PushMessage) {}

        override fun onError(error: Error) {}
    }
}