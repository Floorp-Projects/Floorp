/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.history

import android.view.ViewGroup
import io.mockk.MockKAnnotations
import io.mockk.mockk
import io.mockk.spyk
import io.reactivex.Observer
import io.reactivex.observers.TestObserver
import org.junit.jupiter.api.BeforeEach
import org.junit.jupiter.api.Test
import org.mozilla.fenix.TestUtils.bus
import org.mozilla.fenix.TestUtils.owner
import org.mozilla.fenix.TestUtils.setRxSchedulers
import org.mozilla.fenix.mvi.ActionBusFactory
import org.mozilla.fenix.mvi.UIView
import org.mozilla.fenix.mvi.getManagedEmitter

class HistoryComponentTest {

    private lateinit var historyComponent: TestHistoryComponent
    private lateinit var historyObserver: TestObserver<HistoryState>
    private lateinit var emitter: Observer<HistoryChange>

    @BeforeEach
    fun setup() {
        MockKAnnotations.init(this)
        setRxSchedulers()

        historyComponent = spyk(
            TestHistoryComponent(mockk(), bus),
            recordPrivateCalls = true
        )
        historyObserver = historyComponent.internalRender(historyComponent.reducer).test()
        emitter = owner.getManagedEmitter()
    }

    @Test
    fun `add and remove one history item normally`() {
        val historyItem = HistoryItem(123, "http://mozilla.org", 0)

        emitter.onNext(HistoryChange.Change(listOf(historyItem)))
        emitter.onNext(HistoryChange.EnterEditMode(historyItem))
        emitter.onNext(HistoryChange.RemoveItemForRemoval(historyItem))
        emitter.onNext(HistoryChange.AddItemForRemoval(historyItem))
        emitter.onNext(HistoryChange.ExitEditMode)

        historyObserver.assertSubscribed().awaitCount(6).assertNoErrors()
            .assertValues(
                HistoryState(listOf(), HistoryState.Mode.Normal),
                HistoryState(listOf(historyItem), HistoryState.Mode.Normal),
                HistoryState(listOf(historyItem), HistoryState.Mode.Editing(listOf(historyItem))),
                HistoryState(listOf(historyItem), HistoryState.Mode.Editing(listOf())),
                HistoryState(listOf(historyItem), HistoryState.Mode.Editing(listOf(historyItem))),
                HistoryState(listOf(historyItem), HistoryState.Mode.Normal)
            )
    }

    @Test
    fun `try making changes when not in edit mode`() {
        val historyItems = listOf(
            HistoryItem(1337, "http://reddit.com", 0),
            HistoryItem(31337, "http://leethaxor.com", 0)
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

    @Suppress("MemberVisibilityCanBePrivate")
    class TestHistoryComponent(container: ViewGroup, bus: ActionBusFactory) :
        HistoryComponent(container, bus) {

        override val uiView: UIView<HistoryState, HistoryAction, HistoryChange>
            get() = mockk(relaxed = true)
    }
}