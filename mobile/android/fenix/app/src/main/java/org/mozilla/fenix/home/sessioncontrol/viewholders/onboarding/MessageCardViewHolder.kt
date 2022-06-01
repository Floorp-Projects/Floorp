/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.sessioncontrol.viewholders.onboarding

import android.view.View
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.ui.platform.ComposeView
import androidx.lifecycle.LifecycleOwner
import org.mozilla.fenix.R
import org.mozilla.fenix.compose.ComposeViewHolder
import org.mozilla.fenix.compose.MessageCard
import org.mozilla.fenix.gleanplumb.Message
import org.mozilla.fenix.home.sessioncontrol.SessionControlInteractor

/**
 * View holder for the Nimbus Message Card.
 *
 * @property interactor [SessionControlInteractor] which will have delegated to all user
 * interactions.
 */
class MessageCardViewHolder(
    composeView: ComposeView,
    viewLifecycleOwner: LifecycleOwner,
    private val interactor: SessionControlInteractor,
) : ComposeViewHolder(composeView, viewLifecycleOwner) {
    private lateinit var messageGlobal: Message

    companion object {
        internal val LAYOUT_ID = View.generateViewId()
    }

    init {
        val horizontalPadding =
            composeView.resources.getDimensionPixelSize(R.dimen.home_item_horizontal_margin)
        composeView.setPadding(horizontalPadding, 0, horizontalPadding, 0)
    }

    fun bind(message: Message) {
        messageGlobal = message
    }

    @Composable
    override fun Content() {
        val message by remember { mutableStateOf(messageGlobal) }

        MessageCard(
            message = message,
            onClick = { interactor.onMessageClicked(message) },
            onCloseButtonClick = { interactor.onMessageClosedClicked(message) }
        )
    }
}
