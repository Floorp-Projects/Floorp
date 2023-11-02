/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.shopping

import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.filter
import kotlinx.coroutines.flow.map
import kotlinx.coroutines.flow.scan
import mozilla.components.lib.state.helpers.AbstractBinding
import org.mozilla.fenix.shopping.store.BottomSheetViewState
import org.mozilla.fenix.shopping.store.ReviewQualityCheckState
import org.mozilla.fenix.shopping.store.ReviewQualityCheckStore

/**
 * View-bound feature that requests the bottom sheet state to be changed to expanded or collapsed when
 * the store state changes from [ReviewQualityCheckState.Initial] to [ReviewQualityCheckState.NotOptedIn].
 *
 * @param store The store to observe.
 * @param onRequestStateUpdate Callback to request the bottom sheet to be updated.
 */
class ReviewQualityCheckBottomSheetStateFeature(
    store: ReviewQualityCheckStore,
    private val onRequestStateUpdate: (expanded: BottomSheetViewState) -> Unit,
) : AbstractBinding<ReviewQualityCheckState>(store) {
    override suspend fun onState(flow: Flow<ReviewQualityCheckState>) {
        val initial = Pair<ReviewQualityCheckState?, ReviewQualityCheckState?>(null, null)
        flow.scan(initial) { acc, value ->
            Pair(acc.second, value)
        }.filter {
            it.first is ReviewQualityCheckState.Initial
        }.map {
            when (it.second) {
                is ReviewQualityCheckState.NotOptedIn -> BottomSheetViewState.FULL_VIEW
                else -> BottomSheetViewState.HALF_VIEW
            }
        }.collect {
            onRequestStateUpdate(it)
        }
    }
}
