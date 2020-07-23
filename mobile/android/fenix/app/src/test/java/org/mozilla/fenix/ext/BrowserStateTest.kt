/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ext

import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.MediaState
import mozilla.components.browser.state.state.createTab
import org.junit.Assert.assertEquals
import org.junit.Test

private const val SESSION_ID_MOZILLA = "0"
private const val SESSION_ID_BCC = "1"
private const val SESSION_ID_BAD = "not a real session id"

class BrowserStateTest {

    private val sessionMozilla = createTab(url = "www.mozilla.org", id = SESSION_ID_MOZILLA)
    private val sessionBcc = createTab(url = "www.bcc.co.uk", id = SESSION_ID_BCC)

    @Test
    fun `return media state if it matches tab id`() {
        val state = BrowserState(
            tabs = listOf(sessionBcc, sessionMozilla),
            media = MediaState(
                MediaState.Aggregate(
                state = MediaState.State.PLAYING,
                activeTabId = SESSION_ID_MOZILLA
            ))
        )

        assertEquals(MediaState.State.PLAYING, state.getMediaStateForSession(SESSION_ID_MOZILLA))
        assertEquals(MediaState.State.NONE, state.getMediaStateForSession(SESSION_ID_BCC))
        assertEquals(MediaState.State.NONE, state.getMediaStateForSession(SESSION_ID_BAD))
    }
}
