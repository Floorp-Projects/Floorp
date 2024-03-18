/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.shopping.middleware

import junit.framework.TestCase.assertEquals
import kotlinx.coroutines.test.runTest
import org.junit.Test

class RetryKtTest {

    @Test
    fun `WHEN predicate is false THEN return data on first attempt`() = runTest {
        var count = 0

        val actual = retry(predicate = { false }) {
            count += 1
            count
        }
        val expected = 1

        assertEquals(expected, actual)
    }

    @Test
    fun `WHEN predicate is true THEN retry max times`() = runTest {
        var count = 0

        val actual = retry(
            maxRetries = 10,
            predicate = { true },
        ) {
            count += 1
            count
        }

        val expected = 10

        assertEquals(expected, actual)
    }

    @Test
    fun `WHEN predicate changes to false from true THEN return data on that attempt`() = runTest {
        var count = 0

        val actual = retry(
            maxRetries = 10,
            predicate = { it < 5 },
        ) {
            count += 1
            count
        }

        val expected = 5

        assertEquals(expected, actual)
    }
}
