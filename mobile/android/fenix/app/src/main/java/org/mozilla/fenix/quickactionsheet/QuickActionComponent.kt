/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.quickactionsheet

import android.view.ViewGroup
import androidx.fragment.app.Fragment
import androidx.lifecycle.ViewModel
import androidx.lifecycle.ViewModelProvider
import io.reactivex.Observable
import org.mozilla.fenix.mvi.Action
import org.mozilla.fenix.mvi.ActionBusFactory
import org.mozilla.fenix.mvi.Change
import org.mozilla.fenix.mvi.Reducer
import org.mozilla.fenix.mvi.UIComponent
import org.mozilla.fenix.mvi.UIComponentViewModel
import org.mozilla.fenix.mvi.UIView
import org.mozilla.fenix.mvi.ViewState

class QuickActionComponent(
    private val container: ViewGroup,
    owner: Fragment,
    bus: ActionBusFactory,
    override var initialState: QuickActionState = QuickActionState(
        readable = false,
        bookmarked = false,
        readerActive = false,
        bounceNeeded = false
    )
) : UIComponent<QuickActionState, QuickActionAction, QuickActionChange>(
    owner,
    bus.getManagedEmitter(QuickActionAction::class.java),
    bus.getSafeManagedObservable(QuickActionChange::class.java)
) {
    override fun initView(): UIView<QuickActionState, QuickActionAction, QuickActionChange> =
        QuickActionUIView(container, actionEmitter, changesObservable)

    override fun render(): Observable<QuickActionState> =
        ViewModelProvider(
            owner,
            QuickActionViewModel.Factory(initialState, changesObservable)
        ).get(QuickActionViewModel::class.java).render(uiView)

    init {
        render()
    }
}

data class QuickActionState(
    val readable: Boolean,
    val bookmarked: Boolean,
    val readerActive: Boolean,
    val bounceNeeded: Boolean
) : ViewState

sealed class QuickActionAction : Action {
    object Opened : QuickActionAction()
    object Closed : QuickActionAction()
    object SharePressed : QuickActionAction()
    object DownloadsPressed : QuickActionAction()
    object BookmarkPressed : QuickActionAction()
    object ReadPressed : QuickActionAction()
    object ReadAppearancePressed : QuickActionAction()
}

sealed class QuickActionChange : Change {
    data class BookmarkedStateChange(val bookmarked: Boolean) : QuickActionChange()
    data class ReadableStateChange(val readable: Boolean) : QuickActionChange()
    data class ReaderActiveStateChange(val active: Boolean) : QuickActionChange()
    object BounceNeededChange : QuickActionChange()
}

class QuickActionViewModel(initialState: QuickActionState, changesObservable: Observable<QuickActionChange>) :
    UIComponentViewModel<QuickActionState, QuickActionAction, QuickActionChange>(
        initialState,
        changesObservable,
        reducer
    ) {

    class Factory(
        private val initialState: QuickActionState,
        private val changesObservable: Observable<QuickActionChange>
    ) : ViewModelProvider.Factory {
        @Suppress("UNCHECKED_CAST")
        override fun <T : ViewModel?> create(modelClass: Class<T>): T =
            QuickActionViewModel(initialState, changesObservable) as T
    }

    companion object {
        val reducer: Reducer<QuickActionState, QuickActionChange> = { state, change ->
            when (change) {
                is QuickActionChange.BounceNeededChange -> {
                    state.copy(bounceNeeded = true)
                }
                is QuickActionChange.BookmarkedStateChange -> {
                    state.copy(bookmarked = change.bookmarked)
                }
                is QuickActionChange.ReadableStateChange -> {
                    state.copy(readable = change.readable)
                }
                is QuickActionChange.ReaderActiveStateChange -> {
                    state.copy(readerActive = change.active)
                }
            }
        }
    }
}
