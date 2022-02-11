/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.history

import mozilla.components.concept.engine.EngineSession.LoadUrlFlags
import org.mozilla.fenix.BrowserDirection
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.components.metrics.MetricController

/**
 * An interface that handles the view manipulation of the History Search, triggered by the Interactor
 */
interface HistorySearchController {
    fun handleEditingCancelled()
    fun handleTextChanged(text: String)
    fun handleUrlTapped(url: String, flags: LoadUrlFlags = LoadUrlFlags.none())
}

class HistorySearchDialogController(
    private val activity: HomeActivity,
    private val metrics: MetricController,
    private val fragmentStore: HistorySearchFragmentStore,
    private val clearToolbarFocus: () -> Unit,
) : HistorySearchController {

    override fun handleEditingCancelled() {
        clearToolbarFocus()
    }

    override fun handleTextChanged(text: String) {
        fragmentStore.dispatch(HistorySearchFragmentAction.UpdateQuery(text))
    }

    override fun handleUrlTapped(url: String, flags: LoadUrlFlags) {
        metrics.track(Event.HistorySearchResultTapped)
        clearToolbarFocus()

        activity.openToBrowserAndLoad(
            searchTermOrURL = url,
            newTab = true,
            from = BrowserDirection.FromHistorySearchDialog,
            flags = flags
        )
    }
}
