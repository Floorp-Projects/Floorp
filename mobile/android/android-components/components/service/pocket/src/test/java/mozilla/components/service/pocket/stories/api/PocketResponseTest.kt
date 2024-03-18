/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket.stories.api

import org.junit.Assert.assertSame
import org.junit.Assert.assertTrue
import org.junit.Test

class PocketResponseTest {
    @Test
    fun `GIVEN a null argument WHEN wrap is called THEN a Failure is returned`() {
        assertTrue(PocketResponse.wrap(null) is PocketResponse.Failure)
    }

    @Test
    fun `GIVEN an empty Collection argument WHEN wrap is called THEN a Failure is returned`() {
        assertTrue(PocketResponse.wrap(emptyList<Any>()) is PocketResponse.Failure<*>)
    }

    @Test
    fun `GIVEN a not empty Collection argument WHEN wrap is called THEN a Success wrapping that argument is returned`() {
        val argument = listOf(1)

        val result = PocketResponse.wrap(argument)

        assertTrue(result is PocketResponse.Success)
        assertSame(argument, (result as PocketResponse.Success).data)
    }

    @Test
    fun `GIVEN an empty String argument WHEN wrap is called THEN a Failure is returned`() {
        assertTrue(PocketResponse.wrap("") is PocketResponse.Failure<String>)
    }

    @Test
    fun `GIVEN a not empty String argument WHEN wrap is called THEN a Success wrapping that argument is returned`() {
        val argument = "not empty"

        val result = PocketResponse.wrap(argument)

        assertTrue(result is PocketResponse.Success)
        assertSame(argument, (result as PocketResponse.Success).data)
    }

    @Test
    fun `GIVEN a random argument WHEN wrap is called THEN a Success wrapping that argument is returned`() {
        val argument = 42

        val result = PocketResponse.wrap(argument)

        assertTrue(result is PocketResponse.Success)
        assertSame(argument, (result as PocketResponse.Success).data)
    }
}
