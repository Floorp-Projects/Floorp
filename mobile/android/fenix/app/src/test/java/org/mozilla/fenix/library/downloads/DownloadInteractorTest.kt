/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.downloads

import io.mockk.every
import io.mockk.mockk
import io.mockk.verifyAll
import mozilla.components.browser.state.state.content.DownloadState
import org.junit.Assert.assertTrue
import org.junit.Test

class DownloadInteractorTest {
    private val downloadItem = DownloadItem("0", "title", "url", "5.6 mb", "png", DownloadState.Status.COMPLETED)
    val controller: DownloadController = mockk(relaxed = true)
    val interactor = DownloadInteractor(controller)

    @Test
    fun onOpen() {
        interactor.open(downloadItem)

        verifyAll {
            controller.handleOpen(downloadItem)
        }
    }

    @Test
    fun onBackPressed() {
        every {
            controller.handleBackPressed()
        } returns true

        val backpressHandled = interactor.onBackPressed()

        verifyAll {
            controller.handleBackPressed()
        }
        assertTrue(backpressHandled)
    }
}
