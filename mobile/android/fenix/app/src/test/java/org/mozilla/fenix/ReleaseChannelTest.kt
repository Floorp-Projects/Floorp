/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix

import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.mozilla.fenix.ReleaseChannel.Debug

class ReleaseChannelTest {

    @Test
    fun `isReleased and isDebug channels are mutually exclusive`() {
        val debugChannels = setOf(
            Debug
        )

        val nonDebugChannels = ReleaseChannel.values().toSet() - debugChannels

        nonDebugChannels.forEach {
            val className = it.javaClass.simpleName
            assertTrue(className, it.isReleased)
            assertFalse(className, it.isDebug)
        }

        debugChannels.forEach {
            val className = it.javaClass.simpleName
            assertFalse(className, it.isReleased)
            assertTrue(className, it.isDebug)
        }
    }
}
