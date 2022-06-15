/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.sessioncontrol.viewholders.onboarding

import android.view.View
import androidx.core.view.isVisible
import androidx.recyclerview.widget.RecyclerView
import org.mozilla.fenix.R
import org.mozilla.fenix.databinding.NimbusMessageCardBinding
import org.mozilla.fenix.ext.increaseTapArea
import org.mozilla.fenix.gleanplumb.Message
import org.mozilla.fenix.home.sessioncontrol.SessionControlInteractor

class MessageCardViewHolder(
    view: View,
    private val interactor: SessionControlInteractor
) : RecyclerView.ViewHolder(view) {

    fun bind(message: Message) {
        val binding = NimbusMessageCardBinding.bind(itemView)

        if (message.data.title.isNullOrBlank()) {
            binding.titleText.isVisible = false
        } else {
            binding.titleText.text = message.data.title
        }

        binding.descriptionText.text = message.data.text

        if (message.data.buttonLabel.isNullOrBlank()) {
            binding.messageButton.isVisible = false
            binding.experimentCard.setOnClickListener {
                interactor.onMessageClicked(message)
            }
        } else {
            binding.messageButton.text = message.data.buttonLabel
            binding.messageButton.setOnClickListener {
                interactor.onMessageClicked(message)
            }
        }

        binding.close.apply {
            increaseTapArea(CLOSE_BUTTON_EXTRA_DPS)
            setOnClickListener {
                interactor.onMessageClosedClicked(message)
            }
        }
    }

    companion object {
        internal const val LAYOUT_ID = R.layout.nimbus_message_card
        private const val CLOSE_BUTTON_EXTRA_DPS = 38
    }
}
