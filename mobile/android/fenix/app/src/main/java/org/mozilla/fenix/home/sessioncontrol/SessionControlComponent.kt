/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.sessioncontrol

import android.content.Context
import android.graphics.Bitmap
import android.view.View
import android.view.ViewGroup
import androidx.recyclerview.widget.RecyclerView
import io.reactivex.Observer
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.feature.media.state.MediaState
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.home.Mode
import org.mozilla.fenix.mvi.Action
import org.mozilla.fenix.mvi.ActionBusFactory
import org.mozilla.fenix.mvi.Change
import org.mozilla.fenix.mvi.UIComponent
import org.mozilla.fenix.mvi.UIComponentViewModelBase
import org.mozilla.fenix.mvi.UIComponentViewModelProvider
import org.mozilla.fenix.mvi.ViewState
import mozilla.components.feature.tab.collections.Tab as ComponentTab
import mozilla.components.feature.tab.collections.TabCollection as ACTabCollection

class SessionControlComponent(
    private val container: ViewGroup,
    bus: ActionBusFactory,
    viewModelProvider: UIComponentViewModelProvider<SessionControlState, SessionControlChange>
) :
    UIComponent<SessionControlState, SessionControlAction, SessionControlChange>(
        bus.getManagedEmitter(SessionControlAction::class.java),
        bus.getSafeManagedObservable(SessionControlChange::class.java),
        viewModelProvider
    ) {

    override fun initView() = SessionControlUIView(container, actionEmitter, changesObservable)

    val view: RecyclerView
        get() = uiView.view as RecyclerView

    init {
        bind()
    }
}

data class Tab(
    val sessionId: String,
    val url: String,
    val hostname: String,
    val title: String,
    val selected: Boolean? = null,
    var mediaState: MediaState? = null,
    val icon: Bitmap? = null
)

fun List<Tab>.toSessionBundle(context: Context): MutableList<Session> =
    this.toSessionBundle(context.components.core.sessionManager)

fun List<Tab>.toSessionBundle(sessionManager: SessionManager): MutableList<Session> {
    val sessionBundle = mutableListOf<Session>()
    this.forEach {
        sessionManager.findSessionById(it.sessionId)?.let { session ->
            sessionBundle.add(session)
        }
    }
    return sessionBundle
}

data class SessionControlState(
    val tabs: List<Tab>,
    val expandedCollections: Set<Long>,
    val collections: List<TabCollection>,
    val mode: Mode
) : ViewState

typealias TabCollection = ACTabCollection

sealed class TabAction : Action {
    data class SaveTabGroup(val selectedTabSessionId: String?) : TabAction()
    object ShareTabs : TabAction()
    data class CloseAll(val private: Boolean) : TabAction()
    data class Select(val tabView: View, val sessionId: String) : TabAction()
    data class Close(val sessionId: String) : TabAction()
    data class Share(val sessionId: String) : TabAction()
    data class PauseMedia(val sessionId: String) : TabAction()
    data class PlayMedia(val sessionId: String) : TabAction()
    object PrivateBrowsingLearnMore : TabAction()
}

sealed class CollectionAction : Action {
    data class Expand(val collection: TabCollection) : CollectionAction()
    data class Collapse(val collection: TabCollection) : CollectionAction()
    data class Delete(val collection: TabCollection) : CollectionAction()
    data class AddTab(val collection: TabCollection) : CollectionAction()
    data class Rename(val collection: TabCollection) : CollectionAction()
    data class OpenTab(val tab: ComponentTab) : CollectionAction()
    data class OpenTabs(val collection: TabCollection) : CollectionAction()
    data class ShareTabs(val collection: TabCollection) : CollectionAction()
    data class RemoveTab(val collection: TabCollection, val tab: ComponentTab) : CollectionAction()
}

sealed class OnboardingAction : Action {
    object Finish : OnboardingAction()
}

sealed class SessionControlAction : Action {
    data class Tab(val action: TabAction) : SessionControlAction()
    data class Collection(val action: CollectionAction) : SessionControlAction()
    data class Onboarding(val action: OnboardingAction) : SessionControlAction()
}

fun Observer<SessionControlAction>.onNext(tabAction: TabAction) {
    onNext(SessionControlAction.Tab(tabAction))
}

fun Observer<SessionControlAction>.onNext(collectionAction: CollectionAction) {
    onNext(SessionControlAction.Collection(collectionAction))
}

fun Observer<SessionControlAction>.onNext(onboardingAction: OnboardingAction) {
    onNext(SessionControlAction.Onboarding(onboardingAction))
}

sealed class SessionControlChange : Change {
    data class Change(val tabs: List<Tab>, val mode: Mode, val collections: List<TabCollection>) :
        SessionControlChange()
    data class TabsChange(val tabs: List<Tab>) : SessionControlChange()
    data class ModeChange(val mode: Mode) : SessionControlChange()
    data class CollectionsChange(val collections: List<TabCollection>) : SessionControlChange()
    data class ExpansionChange(val collection: TabCollection, val expand: Boolean) : SessionControlChange()
}

class SessionControlViewModel(
    initialState: SessionControlState
) : UIComponentViewModelBase<SessionControlState, SessionControlChange>(initialState, reducer) {
    companion object {
        val reducer: (SessionControlState, SessionControlChange) -> SessionControlState = { state, change ->
            when (change) {
                is SessionControlChange.CollectionsChange -> state.copy(collections = change.collections)
                is SessionControlChange.TabsChange -> state.copy(tabs = change.tabs)
                is SessionControlChange.ModeChange -> state.copy(mode = change.mode)
                is SessionControlChange.ExpansionChange -> {
                    val newExpandedCollection = state.expandedCollections.toMutableSet()

                    if (change.expand) {
                        newExpandedCollection.add(change.collection.id)
                    } else {
                        newExpandedCollection.remove(change.collection.id)
                    }

                    state.copy(expandedCollections = newExpandedCollection)
                }
                is SessionControlChange.Change -> state.copy(
                    tabs = change.tabs,
                    mode = change.mode,
                    collections = change.collections
                )
            }
        }
    }
}
