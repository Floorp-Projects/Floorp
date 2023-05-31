/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.compose

import androidx.compose.foundation.gestures.FlingBehavior
import androidx.compose.foundation.gestures.ScrollScope
import androidx.compose.foundation.lazy.LazyListState
import androidx.compose.foundation.lazy.LazyRow
import androidx.compose.runtime.Composable
import androidx.compose.runtime.rememberCoroutineScope
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.launch

/**
 * [FlingBehavior] for a [LazyRow] that will automatically scroll the list in the fling direction
 * to fully show the next item.
 */
@Composable
fun EagerFlingBehavior(
    lazyRowState: LazyListState,
): FlingBehavior {
    val scope = rememberCoroutineScope()

    return LazyListEagerFlingBehavior(lazyRowState, scope)
}

private class LazyListEagerFlingBehavior(
    private val lazyRowState: LazyListState,
    private val scope: CoroutineScope,
) : FlingBehavior {
    override suspend fun ScrollScope.performFling(initialVelocity: Float): Float {
        if (lazyRowState.canScrollForward) {
            val firstItemIndex = lazyRowState.firstVisibleItemIndex

            val itemIndexToScrollTo = when (initialVelocity <= 0) {
                true -> firstItemIndex
                false -> firstItemIndex + 1
            }

            scope.launch {
                lazyRowState.animateScrollToItem(itemIndexToScrollTo)
            }
        }

        return 0f // we've consumed the entire fling
    }
}
