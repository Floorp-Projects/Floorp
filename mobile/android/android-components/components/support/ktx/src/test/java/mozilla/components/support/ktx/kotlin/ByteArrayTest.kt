/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.ktx.kotlin

import org.junit.Assert.assertEquals
import org.junit.Test

class ByteArrayTest {

    @Test
    fun toHexString() {
        val stringValue = "Android Components"
        assertEquals("416e64726f696420436f6d706f6e656e7473", stringValue.toByteArray().toHexString())
        assertEquals("416e64726f696420436f6d706f6e656e7473", stringValue.toByteArray().toHexString(-1))
        assertEquals("416e64726f696420436f6d706f6e656e7473", stringValue.toByteArray().toHexString(36))
        assertEquals("00416e64726f696420436f6d706f6e656e7473", stringValue.toByteArray().toHexString(38))
    }

    @Test
    fun toSha256Digest() {
        val stringValue = "Android Components"
        assertEquals(
            "d2d01f10a9700b60740bdd20c60839dcf6b82be9e6a02719d564146cbe32d68f",
            stringValue.toByteArray().toSha256Digest().toHexString(),
        )
    }
}
