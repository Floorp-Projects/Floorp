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

    @Test
    fun `normalSessionSize only counts non-private non-custom sessions`() {
        val normal1 = mockSession()
        val normal2 = mockSession()
        val normal3 = mockSession()

        val private1 = mockSession(isPrivate = true)
        val private2 = mockSession(isPrivate = true)

        val custom1 = mockSession(isCustom = true)
        val custom2 = mockSession(isCustom = true)
        val custom3 = mockSession(isCustom = true)

        val privateCustom = mockSession(isPrivate = true, isCustom = true)

        val sessionManager = mockSessionManager(
            listOf(
                normal1, private1, private2, custom1,
                normal2, normal3, custom2, custom3, privateCustom
            )
        )

        assertEquals(3, sessionManager.normalSessionSize())
    }

    private fun mockSessionManager(sessions: List<Session>): SessionManager {
        val sessionManager: SessionManager = mockk()
        every { sessionManager.sessions } returns sessions
        return sessionManager
    }

    private fun mockSession(
        sessionId: String? = null,
        isPrivate: Boolean = false,
        isCustom: Boolean = false
    ) = mockk<Session> {
        sessionId?.let { every { id } returns it }
        every { private } returns isPrivate
        every { isCustomTabSession() } returns isCustom
    }
}
