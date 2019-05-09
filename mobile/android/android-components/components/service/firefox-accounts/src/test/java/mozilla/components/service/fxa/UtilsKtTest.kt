/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.fxa

import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Assert.fail
import org.junit.Test

class UtilsKtTest {
    @Test
    fun `maybeExceptional returns correct data back`() {
        assertEquals(1, maybeExceptional({
            1
        }, {
            fail()
        }))

        assertEquals("Hello", maybeExceptional({
            "Hello"
        }, {
            fail()
        }))
    }

    @Test
    fun `maybeExceptional does not swallow non-panics`() {
        // Network.
        assertEquals("pass!", maybeExceptional({
            throw FxaNetworkException("oops")
        }, {
            assertEquals("oops", it.message)
            assertTrue(it is FxaNetworkException)
            "pass!"
        }))

        assertEquals("pass!", maybeExceptional({
            throw FxaUnauthorizedException("auth!")
        }, {
            assertEquals("auth!", it.message)
            assertTrue(it is FxaUnauthorizedException)
            "pass!"
        }))

        assertEquals("pass!", maybeExceptional({
            throw FxaUnspecifiedException("dunno")
        }, {
            assertEquals("dunno", it.message)
            assertTrue(it is FxaUnspecifiedException)
            "pass!"
        }))
    }

    @Test(expected = IllegalStateException::class)
    fun `maybeExceptional re-throws non-fxa exceptions`() {
        maybeExceptional({
            throw IllegalStateException("bad state")
        }, {
            fail()
        })
    }

    @Test(expected = FxaPanicException::class)
    fun `maybeExceptional re-throws fxa panic exceptions`() {
        maybeExceptional({
            throw FxaPanicException("don't panic!")
        }, {
            fail()
        })
    }
}