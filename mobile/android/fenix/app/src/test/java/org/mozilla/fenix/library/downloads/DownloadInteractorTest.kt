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
    private val downloadItem = DownloadItem(
        id = "0",
        url = "url",
        fileName = "title",
        filePath = "filePath",
        size = "5.6 mb",
        contentType = "png",
        status = DownloadState.Status.COMPLETED
    )
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
    fun onSelect() {
        interactor.select(downloadItem)

        verifyAll {
            controller.handleSelect(downloadItem)
        }
    }

    @Test
    fun onDeselect() {
        interactor.deselect(downloadItem)

        verifyAll {
            controller.handleDeselect(downloadItem)
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

    @Test
    fun onModeSwitched() {
        interactor.onModeSwitched()

        verifyAll {
            controller.handleModeSwitched()
        }
    }

    @Test
    fun onDeleteAll() {
        interactor.onDeleteAll()

        verifyAll {
            controller.handleDeleteAll()
        }
    }

    @Test
    fun onDeleteSome() {
        val items = setOf(downloadItem)

        interactor.onDeleteSome(items)
        verifyAll {
            controller.handleDeleteSome(items)
        }
    }
}
