/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.history

import io.mockk.every
import io.mockk.mockk
import io.mockk.verifyAll
import org.junit.Assert.assertTrue
import org.junit.Test

class HistoryInteractorTest {
    private val historyItem = History.Regular(0, "title", "url", 0.toLong(), HistoryItemTimeGroup.timeGroupForTimestamp(0))
    val controller: HistoryController = mockk(relaxed = true)
    val interactor = DefaultHistoryInteractor(controller)

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
    fun onSearch() {
        interactor.onSearch()

        verifyAll {
            controller.handleSearch()
        }
    }

    @Test
    fun onDeleteTimeRange() {
        interactor.onDeleteTimeRange()

        verifyAll {
            controller.handleDeleteTimeRange()
        }
    }

    @Test
    fun onDeleteTimeRangeConfirmed() {
        interactor.onDeleteTimeRangeConfirmed(null)

        verifyAll {
            controller.handleDeleteTimeRangeConfirmed(null)
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
