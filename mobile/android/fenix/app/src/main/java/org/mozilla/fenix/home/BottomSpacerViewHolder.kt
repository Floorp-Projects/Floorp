/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home

import android.view.View
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.height
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.ComposeView
import androidx.compose.ui.unit.dp
import androidx.recyclerview.widget.RecyclerView
import org.mozilla.fenix.theme.FirefoxTheme

class BottomSpacerViewHolder(val composeView: ComposeView) : RecyclerView.ViewHolder(composeView) {
    init {
        composeView.setContent {
            FirefoxTheme {
                Spacer(modifier = Modifier.height(88.dp))
            }
        }
    }

    companion object {
        val LAYOUT_ID = View.generateViewId()
    }
}
