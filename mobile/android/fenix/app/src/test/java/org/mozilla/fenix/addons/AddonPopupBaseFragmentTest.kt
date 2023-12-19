/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.addons

import io.mockk.every
import io.mockk.mockk
import io.mockk.spyk
import io.mockk.verify
import mozilla.components.browser.state.action.ContentAction
import mozilla.components.browser.state.state.SessionState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.fetch.Response
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner

@RunWith(FenixRobolectricTestRunner::class)
class AddonPopupBaseFragmentTest {

    private lateinit var fragment: AddonPopupBaseFragment

    @Before
    fun setup() {
        fragment = spyk()
    }

    @Test
    fun `WHEN onExternalResource is call THEN  dispatch an UpdateDownloadAction`() {
        val store = mockk<BrowserStore>(relaxed = true)
        val session = mockk<SessionState>(relaxed = true)
        val response = mockk<Response>(relaxed = true)
        every { fragment.provideBrowserStore() } returns store

        fragment.session = session

        fragment.onExternalResource(
            url = "url",
            fileName = "fileName",
            contentLength = 1,
            contentType = "contentType",
            userAgent = "userAgent",
            isPrivate = true,
            skipConfirmation = false,
            openInApp = false,
            response = response,
        )

        verify { store.dispatch(any<ContentAction.UpdateDownloadAction>()) }
    }
}
