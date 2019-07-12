/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.history

import io.mockk.MockKAnnotations
import io.reactivex.Observer
import io.reactivex.observers.TestObserver
import org.junit.Before
import org.junit.Test
import org.mozilla.fenix.TestUtils.bus
import org.mozilla.fenix.TestUtils.owner
import org.mozilla.fenix.TestUtils.setRxSchedulers
import org.mozilla.fenix.mvi.getManagedEmitter

class HistoryViewModelTest {

    private lateinit var historyViewModel: HistoryViewModel
    private lateinit var historyObserver: TestObserver<HistoryState>
    private lateinit var emitter: Observer<HistoryChange>

    @Before
    fun setup() {
        MockKAnnotations.init(this)
        setRxSchedulers()

        historyViewModel = HistoryViewModel.create()
        historyObserver = historyViewModel.state.test()
        bus.getSafeManagedObservable(HistoryChange::class.java)
            .subscribe(historyViewModel.changes::onNext)

        emitter = owner.getManagedEmitter()
    }

    @Test
    fun `select two items for removal, then deselect one, then select it again`() {
        val historyItem = HistoryItem(1, "Mozilla", "http://mozilla.org", 0)
        val historyItem2 = HistoryItem(2, "Mozilla", "http://mozilla.org", 0)

        emitter.onNext(HistoryChange.Change(listOf(historyItem, historyItem2)))
        emitter.onNext(HistoryChange.EnterEditMode(historyItem))
        emitter.onNext(HistoryChange.AddItemForRemoval(historyItem2))
        emitter.onNext(HistoryChange.RemoveItemForRemoval(historyItem))
        emitter.onNext(HistoryChange.AddItemForRemoval(historyItem))
        emitter.onNext(HistoryChange.ExitEditMode)

        historyObserver.assertSubscribed().awaitCount(7).assertNoErrors()
            .assertValues(
                HistoryState(listOf(), HistoryState.Mode.Normal),
                HistoryState(listOf(historyItem, historyItem2), HistoryState.Mode.Normal),
                HistoryState(listOf(historyItem, historyItem2), HistoryState.Mode.Editing(listOf(historyItem))),
                HistoryState(listOf(historyItem, historyItem2), HistoryState.Mode.Editing(listOf(historyItem, historyItem2))),
                HistoryState(listOf(historyItem, historyItem2), HistoryState.Mode.Editing(listOf(historyItem2))),
                HistoryState(listOf(historyItem, historyItem2), HistoryState.Mode.Editing(listOf(historyItem2, historyItem))),
                HistoryState(listOf(historyItem, historyItem2), HistoryState.Mode.Normal)
            )
    }
    @Test
    fun `deselecting all items triggers normal mode`() {
        val historyItem = HistoryItem(123, "Mozilla", "http://mozilla.org", 0)

        emitter.onNext(HistoryChange.Change(listOf(historyItem)))
        emitter.onNext(HistoryChange.EnterEditMode(historyItem))
        emitter.onNext(HistoryChange.RemoveItemForRemoval(historyItem))
        historyObserver.assertSubscribed().awaitCount(6).assertNoErrors()
            .assertValues(
                HistoryState(listOf(), HistoryState.Mode.Normal),
                HistoryState(listOf(historyItem), HistoryState.Mode.Normal),
                HistoryState(listOf(historyItem), HistoryState.Mode.Editing(listOf(historyItem))),
                HistoryState(listOf(historyItem), HistoryState.Mode.Normal)
            )
    }

    @Test
    fun `try making changes when not in edit mode`() {
        val historyItems = listOf(
            HistoryItem(1337, "Reddit", "http://reddit.com", 0),
            HistoryItem(31337, "Haxor", "http://leethaxor.com", 0)
        )

        emitter.onNext(HistoryChange.Change(historyItems))
        emitter.onNext(HistoryChange.AddItemForRemoval(historyItems[0]))
        emitter.onNext(HistoryChange.EnterEditMode(historyItems[0]))
        emitter.onNext(HistoryChange.ExitEditMode)

        historyObserver.assertSubscribed().awaitCount(4).assertNoErrors()
            .assertValues(
                HistoryState(listOf(), HistoryState.Mode.Normal),
                HistoryState(historyItems, HistoryState.Mode.Normal),
                HistoryState(historyItems, HistoryState.Mode.Editing(listOf(historyItems[0]))),
                HistoryState(historyItems, HistoryState.Mode.Normal)
            )
    }
}
