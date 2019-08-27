/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ext

import io.mockk.every
import io.mockk.mockk
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import org.junit.Assert.assertEquals
import org.junit.Test

class SessionManagerTest {

    @Test
    fun `returns all normal sessions`() {
        val sessions = listOf(
            Session("https://example.com", private = false),
            Session("https://mozilla.org", private = true),
            Session("https://github.com", private = false)
        )
        val sessionManager = mockSessionManager(sessions)

        assertEquals(
            listOf(sessions[0], sessions[2]),
            sessionManager.sessionsOfType(private = false).toList()
        )
    }

    @Test
    fun `returns all private sessions`() {
        val sessions = listOf(
            Session("https://example.com", private = false),
            Session("https://mozilla.org", private = true),
            Session("https://github.com", private = false)
        )
        val sessionManager = mockSessionManager(sessions)

        assertEquals(
            listOf(sessions[1]),
            sessionManager.sessionsOfType(private = true).toList()
        )
    }

    private fun mockSessionManager(sessions: List<Session>): SessionManager {
        val sessionManager: SessionManager = mockk()
        every { sessionManager.sessions } returns sessions
        return sessionManager
    }
}
