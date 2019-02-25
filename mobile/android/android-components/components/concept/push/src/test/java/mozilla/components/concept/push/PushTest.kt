/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 *  License, v. 2.0. If a copy of the MPL was not distributed with this
 *  file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

package mozilla.components.concept.push

import mozilla.components.support.test.mock
import org.junit.Assert.assertNotNull
import org.junit.Test

import org.mockito.Mockito.verify

class PushTest {

    private val service: PushService = mock()

    @Test
    fun init() {
        val push = Push(service)

        push.init()

        verify(push.service).start()
        assertNotNull(Push.requireInstance)
    }

    @Test(expected = IllegalStateException::class)
    fun `requireInstance throws exception if not initialized`() {
        Push.requireInstance
    }
}