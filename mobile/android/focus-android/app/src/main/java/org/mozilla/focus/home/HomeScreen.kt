/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.home

import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.height
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import kotlinx.coroutines.DelicateCoroutinesApi
import mozilla.components.lib.state.ext.observeAsComposableState
import org.mozilla.focus.components
import org.mozilla.focus.topsites.TopSites

/**
 * The home screen.
 */
@OptIn(DelicateCoroutinesApi::class)
@Composable
fun HomeScreen() {
    val topSitesState = components.appStore.observeAsComposableState { state -> state.topSites }

    Column(horizontalAlignment = Alignment.CenterHorizontally) {
        val topSites = topSitesState.value!!

        if (topSites.isNotEmpty()) {
            Spacer(modifier = Modifier.height(24.dp))

            TopSites(topSites = topSites)
        }
    }
}
