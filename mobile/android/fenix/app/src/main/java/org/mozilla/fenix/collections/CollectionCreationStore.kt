/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.collections

import mozilla.components.feature.tab.collections.TabCollection
import mozilla.components.lib.state.Action
import mozilla.components.lib.state.State
import mozilla.components.lib.state.Store
import org.mozilla.fenix.collections.CollectionCreationAction.StepChanged
import org.mozilla.fenix.home.Tab

class CollectionCreationStore(
    initialState: CollectionCreationState
) : Store<CollectionCreationState, CollectionCreationAction>(
    initialState,
    ::collectionCreationReducer
)

/**
 * Represents the current purpose of the screen. This determines what options are shown to the
 * user.
 *
 * TODO refactor [CollectionCreationState] into a sealed class with four implementations, each
 * replacing a [SaveCollectionStep] value. These will not need null / emptyCollection default
 * values. Handle changes between these state changes internally, here and in the controller,
 * instead of exposing [StepChanged], which currently acts as a setter.
 */
enum class SaveCollectionStep {
    SelectTabs,
    SelectCollection,
    NameCollection,
    RenameCollection
}

data class CollectionCreationState(
    val tabs: List<Tab> = emptyList(),
    val selectedTabs: Set<Tab> = emptySet(),
    val saveCollectionStep: SaveCollectionStep = SaveCollectionStep.SelectTabs,
    val tabCollections: List<TabCollection> = emptyList(),
    val selectedTabCollection: TabCollection? = null,
    val defaultCollectionNumber: Int = 1
) : State

sealed class CollectionCreationAction : Action {
    object AddAllTabs : CollectionCreationAction()
    object RemoveAllTabs : CollectionCreationAction()
    data class TabAdded(val tab: Tab) : CollectionCreationAction()
    data class TabRemoved(val tab: Tab) : CollectionCreationAction()
    /**
     * Used as a setter for [SaveCollectionStep].
     *
     * This should be refactored, see kdoc on [SaveCollectionStep].
     */
    data class StepChanged(
        val saveCollectionStep: SaveCollectionStep,
        val defaultCollectionNumber: Int = 1
    ) : CollectionCreationAction()
}

private fun collectionCreationReducer(
    prevState: CollectionCreationState,
    action: CollectionCreationAction
): CollectionCreationState = when (action) {
    is CollectionCreationAction.AddAllTabs -> prevState.copy(selectedTabs = prevState.tabs.toSet())
    is CollectionCreationAction.RemoveAllTabs -> prevState.copy(selectedTabs = emptySet())
    is CollectionCreationAction.TabAdded -> prevState.copy(selectedTabs = prevState.selectedTabs + action.tab)
    is CollectionCreationAction.TabRemoved -> prevState.copy(selectedTabs = prevState.selectedTabs - action.tab)
    is CollectionCreationAction.StepChanged -> prevState.copy(
        saveCollectionStep = action.saveCollectionStep,
        defaultCollectionNumber = action.defaultCollectionNumber
    )
}
