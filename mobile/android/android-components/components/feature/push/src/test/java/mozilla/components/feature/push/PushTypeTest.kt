/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.push

import org.junit.Assert.assertEquals
import org.junit.Test

class PushTypeTest {

    @Test
    fun `channelId never changes for Services`() {
        assertEquals("992a0f0542383f1ea5ef51b7cf4ae6c4", PushType.Services.toChannelId())
    }

    @Test
    fun `channelId never changes for WebPush`() {
        assertEquals("97a36501975938cea04e226e39dacb55", PushType.WebPush.toChannelId())
    }
}
