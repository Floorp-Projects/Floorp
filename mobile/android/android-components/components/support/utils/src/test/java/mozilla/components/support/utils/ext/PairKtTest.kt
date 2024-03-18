/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import mozilla.components.support.utils.ext.toNullablePair
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Test

class PairKtTest {

    @Test
    fun `a pair with two non-null values should become a non-null pair of non-null types`() {
        val actual = ("hi" as String? to "there" as String?).toNullablePair()

        assertNotNull(actual)
        assertNotNull(actual!!.first)
        assertNotNull(actual.second)
    }

    @Test
    fun `a pair with any null values should become null`() {
        listOf(
            null to "nonNull",
            null to null,
            "nonNull" to null,
        )
            .map { it.toNullablePair() }
            .forEach { assertNull(it) }
    }
}
