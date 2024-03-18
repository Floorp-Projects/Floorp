/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.search

import io.mockk.every
import io.mockk.mockk
import mozilla.components.concept.fetch.Client
import mozilla.components.concept.fetch.MutableHeaders
import mozilla.components.concept.fetch.Request
import mozilla.components.concept.fetch.Response
import org.junit.Assert.assertEquals
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import java.io.ByteArrayInputStream
import java.io.IOException

@RunWith(FenixRobolectricTestRunner::class)
class SearchStringValidatorTest {

    private val client: Client = mockk()

    @Test
    fun `test MDN url`() {
        val request = Request(
            url = "https://developer.mozilla.org/en-US/search?q=1",
        )
        every { client.fetch(request) } returns Response(
            url = "",
            status = 200,
            headers = MutableHeaders(),
            body = Response.Body(ByteArrayInputStream("".toByteArray())),
        )

        val result = SearchStringValidator.isSearchStringValid(client, "https://developer.mozilla.org/en-US/search?q=%s")

        assertEquals(SearchStringValidator.Result.Success, result)
    }

    @Test
    fun `normalize search url`() {
        val request = Request(
            url = "http://firefox.com/search?q=1",
        )
        every { client.fetch(request) } returns Response(
            url = "",
            status = 200,
            headers = MutableHeaders(),
            body = Response.Body(ByteArrayInputStream("".toByteArray())),
        )

        val result = SearchStringValidator.isSearchStringValid(client, "firefox.com/search?q=%s")

        assertEquals(SearchStringValidator.Result.Success, result)
    }

    @Test
    fun `fail if IOException is thrown`() {
        every { client.fetch(any()) } throws IOException()

        val result = SearchStringValidator.isSearchStringValid(client, "https://developer.mozilla.org/en-US/search?q=%s")

        assertEquals(SearchStringValidator.Result.CannotReach, result)
    }

    @Test
    fun `fail if IllegalArgumentException is thrown`() {
        every { client.fetch(any()) } throws IllegalArgumentException()

        val result = SearchStringValidator.isSearchStringValid(client, "https://developer.mozilla.org/en-US/search?q=%s")

        assertEquals(SearchStringValidator.Result.CannotReach, result)
    }

    @Test
    fun `pass if status code is not in success range`() {
        every { client.fetch(any()) } returns Response(
            url = "",
            status = 400,
            headers = MutableHeaders(),
            body = Response.Body(ByteArrayInputStream("".toByteArray())),
        )

        val result = SearchStringValidator.isSearchStringValid(client, "https://developer.mozilla.org/en-US/search?q=%s")

        assertEquals(SearchStringValidator.Result.CannotReach, result)
    }

    @Test
    fun `pass even if 404 status is returned`() {
        every { client.fetch(any()) } returns Response(
            url = "",
            status = 404,
            headers = MutableHeaders(),
            body = Response.Body(ByteArrayInputStream("".toByteArray())),
        )

        val result = SearchStringValidator.isSearchStringValid(client, "https://developer.mozilla.org/en-US/search?q=%s")

        assertEquals(SearchStringValidator.Result.Success, result)
    }
}
