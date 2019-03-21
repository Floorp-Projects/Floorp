/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.bookmarks

import android.view.ViewGroup
import mozilla.components.concept.storage.BookmarkNode
import org.mozilla.fenix.mvi.Action
import org.mozilla.fenix.mvi.ActionBusFactory
import org.mozilla.fenix.mvi.Change
import org.mozilla.fenix.mvi.Reducer
import org.mozilla.fenix.mvi.UIComponent
import org.mozilla.fenix.mvi.UIView
import org.mozilla.fenix.mvi.ViewState

class BookmarkComponent(
    private val container: ViewGroup,
    bus: ActionBusFactory,
    override var initialState: BookmarkState =
        BookmarkState(null, BookmarkState.Mode.Normal)
) :
    UIComponent<BookmarkState, BookmarkAction, BookmarkChange>(
        bus.getManagedEmitter(BookmarkAction::class.java),
        bus.getSafeManagedObservable(BookmarkChange::class.java)
    ) {

    override val reducer: Reducer<BookmarkState, BookmarkChange> = { state, change ->
        when (change) {
            is BookmarkChange.Change -> {
                state.copy(tree = change.tree)
            }
        }
    }

    override fun initView(): UIView<BookmarkState, BookmarkAction, BookmarkChange> =
        BookmarkUIView(container, actionEmitter, changesObservable)

    init {
        render(reducer)
    }
}

data class BookmarkState(val tree: BookmarkNode?, val mode: BookmarkState.Mode) : ViewState {
    sealed class Mode {
        object Normal : Mode()
        data class Selecting(val selectedItems: List<BookmarkNode>) : Mode()
    }
}

sealed class BookmarkAction : Action {
    data class Open(val item: BookmarkNode) : BookmarkAction()
    data class Expand(val folder: BookmarkNode) : BookmarkAction()
    data class Edit(val item: BookmarkNode) : BookmarkAction()
    data class Copy(val item: BookmarkNode) : BookmarkAction()
    data class Share(val item: BookmarkNode) : BookmarkAction()
    data class OpenInNewTab(val item: BookmarkNode) : BookmarkAction()
    data class OpenInPrivateTab(val item: BookmarkNode) : BookmarkAction()
    data class Select(val item: BookmarkNode) : BookmarkAction()
    data class Delete(val item: BookmarkNode) : BookmarkAction()
    object ExitSelectMode : BookmarkAction()
    object BackPressed : BookmarkAction()
}

sealed class BookmarkChange : Change {
    data class Change(val tree: BookmarkNode) : BookmarkChange()
}
