/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.share

import android.view.ViewGroup
import mozilla.components.concept.sync.Device
import org.mozilla.fenix.mvi.Action
import org.mozilla.fenix.mvi.ActionBusFactory
import org.mozilla.fenix.mvi.Change
import org.mozilla.fenix.mvi.Reducer
import org.mozilla.fenix.mvi.UIComponent
import org.mozilla.fenix.mvi.UIComponentViewModelBase
import org.mozilla.fenix.mvi.UIComponentViewModelProvider
import org.mozilla.fenix.mvi.ViewState

object ShareState : ViewState

sealed class ShareChange : Change

sealed class ShareAction : Action {
    object Close : ShareAction()
    object SignInClicked : ShareAction()
    object AddNewDeviceClicked : ShareAction()
    data class ShareDeviceClicked(val device: Device) : ShareAction()
    data class SendAllClicked(val devices: List<Device>) : ShareAction()
    data class ShareAppClicked(val item: ShareItem) : ShareAction()
}

class ShareComponent(
    private val container: ViewGroup,
    bus: ActionBusFactory,
    viewModelProvider: UIComponentViewModelProvider<ShareState, ShareChange>
) : UIComponent<ShareState, ShareAction, ShareChange>(
    bus.getManagedEmitter(ShareAction::class.java),
    bus.getSafeManagedObservable(ShareChange::class.java),
    viewModelProvider
) {
    override fun initView() = ShareUIView(container, actionEmitter, changesObservable)

    init {
        bind()
    }
}

class ShareUIViewModel(
    initialState: ShareState
) : UIComponentViewModelBase<ShareState, ShareChange>(
    initialState,
    reducer
) {
    companion object {
        val reducer: Reducer<ShareState, ShareChange> = { _, _ -> ShareState }
    }
}
