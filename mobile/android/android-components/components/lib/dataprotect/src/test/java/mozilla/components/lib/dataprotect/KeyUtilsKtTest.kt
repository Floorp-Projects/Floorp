/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.dataprotect

import org.junit.Assert.assertEquals
import org.junit.Assert.fail
import org.junit.Test

class KeyUtilsKtTest {
    @Test
    fun `key generation`() {
        try {
            generateEncryptionKey(-1)
            fail()
        } catch (e: IllegalArgumentException) {}

        try {
            generateEncryptionKey(0)
            fail()
        } catch (e: IllegalArgumentException) {}

        try {
            generateEncryptionKey(1)
            fail()
        } catch (e: IllegalArgumentException) {}

        try {
            generateEncryptionKey(32)
            fail()
        } catch (e: IllegalArgumentException) {}

        with(generateEncryptionKey(256)) {
            assertEquals(64, this.toByteArray().size)
        }

        with(generateEncryptionKey(300)) {
            assertEquals(74, this.toByteArray().size)
        }

        with(generateEncryptionKey(512)) {
            assertEquals(128, this.toByteArray().size)
        }
    }
}
