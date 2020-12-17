/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket.api

import mozilla.components.service.pocket.helpers.TEST_VALID_API_KEY
import org.junit.Assert.assertEquals
import org.junit.Test

class ArgumentsTest {
    @Test(expected = IllegalArgumentException::class)
    fun `GIVEN an empty String WHEN assertIsNotBlank is called THEN an exception is thrown`() {
        Arguments.assertIsNotBlank("", "test")
    }

    @Test(expected = IllegalArgumentException::class)
    fun `GIVEN a String containing only spaces String WHEN assertIsNotBlank is called THEN an exception is thrown`() {
        Arguments.assertIsNotBlank("  ", "test")
    }

    @Test
    fun `GIVEN a String containing other characters than space String WHEN assertIsNotBlank is called THEN no exception is thrown`() {
        Arguments.assertIsNotBlank("test", "")
    }

    @Test
    fun `WHEN VALID_API_KEY_PATTERN is used THEN a specific pattern is returned`() {
        val validApiKeyStructure = "[0-9]{5}-[a-zA-Z0-9]{24}"

        assertEquals(validApiKeyStructure, validApiKeyPattern.pattern())
    }

    // Cannot easily test whether the expected Pattern is used. Fallback to test the happy paths.
    @Test
    fun `GIVEN a String with the expected Pocket API key structure WHEN assertApiKeyHasValidStructure is called THEN no exception is thrown`() {
        Arguments.assertApiKeyHasValidStructure(TEST_VALID_API_KEY)
    }

    @Test(expected = IllegalArgumentException::class)
    fun `GIVEN a String with fewer characters than the Pocket API key structure WHEN assertApiKeyHasValidStructure is called THEN an exception is thrown`() {
        val validApiKey = "0".repeat(4) + '-' + "0".repeat(24)

        Arguments.assertApiKeyHasValidStructure(validApiKey)
    }

    @Test(expected = IllegalArgumentException::class)
    fun `GIVEN a String with fewer characters than the Pocket API key structure WHEN assertApiKeyHasValidStructure is called THEN an exception is thrown 2`() {
        val validApiKey = "0".repeat(5) + '-' + "0".repeat(23)

        Arguments.assertApiKeyHasValidStructure(validApiKey)
    }

    @Test(expected = IllegalArgumentException::class)
    fun `GIVEN a String with more characters than the Pocket API key structure WHEN assertApiKeyHasValidStructure is called THEN an exception is thrown`() {
        val validApiKey = "0".repeat(6) + '-' + "0".repeat(24)

        Arguments.assertApiKeyHasValidStructure(validApiKey)
    }

    @Test(expected = IllegalArgumentException::class)
    fun `GIVEN a String with more characters than the Pocket API key structure WHEN assertApiKeyHasValidStructure is called THEN an exception is thrown 2`() {
        val validApiKey = "0".repeat(5) + '-' + "0".repeat(25)

        Arguments.assertApiKeyHasValidStructure(validApiKey)
    }

    @Test(expected = IllegalArgumentException::class)
    fun `GIVEN a String with no separator as expected in the Pocket API key structure WHEN assertApiKeyHasValidStructure is called THEN an exception is thrown`() {
        val validApiKey = "0".repeat(5) + "0".repeat(24)

        Arguments.assertApiKeyHasValidStructure(validApiKey)
    }

    @Test(expected = IllegalArgumentException::class)
    fun `GIVEN a String with a different separator as expected in the Pocket API key structure WHEN assertApiKeyHasValidStructure is called THEN an exception is thrown`() {
        val validApiKey = "0".repeat(5) + '_' + "0".repeat(24)

        Arguments.assertApiKeyHasValidStructure(validApiKey)
    }
}
