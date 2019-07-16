/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.quickactionsheet

import android.view.ViewGroup
import org.mozilla.fenix.mvi.Action
import org.mozilla.fenix.mvi.ActionBusFactory
import org.mozilla.fenix.mvi.UIComponentViewModelProvider
import org.mozilla.fenix.mvi.ViewState
import org.mozilla.fenix.mvi.Change
import org.mozilla.fenix.mvi.Reducer
import org.mozilla.fenix.mvi.UIComponent
import org.mozilla.fenix.mvi.UIComponentViewModelBase
import org.mozilla.fenix.mvi.UIView

class QuickActionComponent(
    private val container: ViewGroup,
    bus: ActionBusFactory,
    viewModelProvider: UIComponentViewModelProvider<QuickActionState, QuickActionChange>
) : UIComponent<QuickActionState, QuickActionAction, QuickActionChange>(
    bus.getManagedEmitter(QuickActionAction::class.java),
    bus.getSafeManagedObservable(QuickActionChange::class.java),
    viewModelProvider
) {
    override fun initView(): UIView<QuickActionState, QuickActionAction, QuickActionChange> =
        QuickActionUIView(container, actionEmitter, changesObservable)

    init {
        bind()
    }
}

data class QuickActionState(
    val readable: Boolean,
    val bookmarked: Boolean,
    val readerActive: Boolean,
    val bounceNeeded: Boolean,
    val isAppLink: Boolean
) : ViewState

sealed class QuickActionAction : Action {
    object Opened : QuickActionAction()
    object Closed : QuickActionAction()
    object SharePressed : QuickActionAction()
    object DownloadsPressed : QuickActionAction()
    object BookmarkPressed : QuickActionAction()
    object ReadPressed : QuickActionAction()
    object ReadAppearancePressed : QuickActionAction()
    object OpenAppLinkPressed : QuickActionAction()
}

sealed class QuickActionChange : Change {
    data class BookmarkedStateChange(val bookmarked: Boolean) : QuickActionChange()
    data class ReadableStateChange(val readable: Boolean) : QuickActionChange()
    data class ReaderActiveStateChange(val active: Boolean) : QuickActionChange()
    data class AppLinkStateChange(val isAppLink: Boolean) : QuickActionChange()
    object BounceNeededChange : QuickActionChange()
}

class QuickActionViewModel(
    initialState: QuickActionState
) : UIComponentViewModelBase<QuickActionState, QuickActionChange>(initialState, reducer) {
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
                is QuickActionChange.AppLinkStateChange -> {
                    state.copy(isAppLink = change.isAppLink)
                }
            }
        }
    }
}
