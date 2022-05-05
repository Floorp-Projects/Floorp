/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.sessioncontrol.viewholders

import android.view.View
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.height
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.ComposeView
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import androidx.lifecycle.LifecycleOwner
import org.mozilla.fenix.R
import org.mozilla.fenix.compose.ComposeViewHolder
import org.mozilla.fenix.compose.button.TertiaryButton
import org.mozilla.fenix.home.sessioncontrol.CustomizeHomeIteractor

class CustomizeHomeButtonViewHolder(
    composeView: ComposeView,
    viewLifecycleOwner: LifecycleOwner,
    private val interactor: CustomizeHomeIteractor
) : ComposeViewHolder(composeView, viewLifecycleOwner) {

    companion object {
        val LAYOUT_ID = View.generateViewId()
    }

    init {
        val horizontalPadding =
            composeView.resources.getDimensionPixelSize(R.dimen.home_item_horizontal_margin)
        composeView.setPadding(horizontalPadding, 0, horizontalPadding, 0)
    }

    @Composable
    override fun Content() {
        Column {
            Spacer(modifier = Modifier.height(68.dp))

            TertiaryButton(
                text = stringResource(R.string.browser_menu_customize_home_1),
                onClick = interactor::openCustomizeHomePage,
            )
        }
    }
}
