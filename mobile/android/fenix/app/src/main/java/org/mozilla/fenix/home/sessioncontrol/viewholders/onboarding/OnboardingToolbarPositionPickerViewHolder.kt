/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.sessioncontrol.viewholders.onboarding

import android.view.View
import androidx.recyclerview.widget.RecyclerView
import org.mozilla.fenix.GleanMetrics.Onboarding
import org.mozilla.fenix.R
import org.mozilla.fenix.components.toolbar.ToolbarPosition
import org.mozilla.fenix.databinding.OnboardingToolbarPositionPickerBinding
import org.mozilla.fenix.ext.asActivity
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.onboarding.OnboardingRadioButton
import org.mozilla.fenix.utils.view.addToRadioGroup

class OnboardingToolbarPositionPickerViewHolder(view: View) : RecyclerView.ViewHolder(view) {

    init {
        val binding = OnboardingToolbarPositionPickerBinding.bind(view)

        val radioTopToolbar = binding.toolbarTopRadioButton
        val radioBottomToolbar = binding.toolbarBottomRadioButton
        val radio: OnboardingRadioButton

        addToRadioGroup(radioTopToolbar, radioBottomToolbar)
        radioTopToolbar.addIllustration(binding.toolbarTopImage)
        radioBottomToolbar.addIllustration(binding.toolbarBottomImage)

        val settings = view.context.components.settings
        radio = when (settings.toolbarPosition) {
            ToolbarPosition.BOTTOM -> radioBottomToolbar
            ToolbarPosition.TOP -> radioTopToolbar
        }
        radio.updateRadioValue(true)

        radioBottomToolbar.onClickListener {
            Onboarding.prefToggledToolbarPosition.record(
                Onboarding.PrefToggledToolbarPositionExtra(
                    Position.BOTTOM.name
                )
            )

            itemView.context.asActivity()?.recreate()
        }

        binding.toolbarBottomImage.setOnClickListener {
            Onboarding.prefToggledToolbarPosition.record(
                Onboarding.PrefToggledToolbarPositionExtra(
                    Position.BOTTOM.name
                )
            )

            radioBottomToolbar.performClick()
        }

        radioTopToolbar.onClickListener {
            Onboarding.prefToggledToolbarPosition.record(
                Onboarding.PrefToggledToolbarPositionExtra(
                    Position.TOP.name
                )
            )
            itemView.context.asActivity()?.recreate()
        }

        binding.toolbarTopImage.setOnClickListener {
            Onboarding.prefToggledToolbarPosition.record(
                Onboarding.PrefToggledToolbarPositionExtra(
                    Position.TOP.name
                )
            )
            radioTopToolbar.performClick()
        }
    }

    companion object {
        const val LAYOUT_ID = R.layout.onboarding_toolbar_position_picker
        // Position of the toolbar used for telemetry
        enum class Position { TOP, BOTTOM }
    }
}
