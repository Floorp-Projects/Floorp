/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.history

import io.mockk.every
import io.mockk.mockk
import io.mockk.verifyAll
import org.junit.Assert.assertTrue
import org.junit.Test
import org.mozilla.fenix.browser.browsingmode.BrowsingMode

class HistoryInteractorTest {
    private val historyItem = HistoryItem(0, "title", "url", 0.toLong())
    val controller: HistoryController = mockk(relaxed = true)
    val interactor = HistoryInteractor(controller)

    @Test
    fun onOpen() {
        interactor.open(historyItem)

        verifyAll {
            controller.handleOpen(historyItem)
        }
    }

    @Test
    fun onSelect() {
        interactor.select(historyItem)

        verifyAll {
            controller.handleSelect(historyItem)
        }
    }

    @Test
    fun onDeselect() {
        interactor.deselect(historyItem)

        verifyAll {
            controller.handleDeselect(historyItem)
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
    fun onCopyPressed() {
        interactor.onCopyPressed(historyItem)

        verifyAll {
            controller.handleCopyUrl(historyItem)
        }
    }

    @Test
    fun onSharePressed() {
        interactor.onSharePressed(historyItem)

        verifyAll {
            controller.handleShare(historyItem)
        }
    }

    @Test
    fun onOpenInNormalTab() {
        interactor.onOpenInNormalTab(historyItem)

        verifyAll {
            controller.handleOpenInNewTab(historyItem, BrowsingMode.Normal)
        }
    }

    @Test
    fun onOpenInPrivateTab() {
        interactor.onOpenInPrivateTab(historyItem)

        verifyAll {
            controller.handleOpenInNewTab(historyItem, BrowsingMode.Private)
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
        val items = setOf(historyItem)

        interactor.onDeleteSome(items)
        verifyAll {
            controller.handleDeleteSome(items)
        }
    }

    @Test
    fun onRequestSync() {
        interactor.onRequestSync()
        verifyAll {
            controller.handleRequestSync()
        }
    }
}
