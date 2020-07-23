/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.sessioncontrol.viewholders.onboarding

import android.view.View
import androidx.recyclerview.widget.RecyclerView
import kotlinx.android.synthetic.main.onboarding_toolbar_position_picker.view.*
import org.mozilla.fenix.R
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.components.metrics.Event.OnboardingToolbarPosition.Position
import org.mozilla.fenix.components.toolbar.ToolbarPosition
import org.mozilla.fenix.ext.asActivity
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.onboarding.OnboardingRadioButton
import org.mozilla.fenix.utils.view.addToRadioGroup

class OnboardingToolbarPositionPickerViewHolder(view: View) : RecyclerView.ViewHolder(view) {

    private val metrics = view.context.components.analytics.metrics

    init {
        val radioTopToolbar = view.toolbar_top_radio_button
        val radioBottomToolbar = view.toolbar_bottom_radio_button
        val radio: OnboardingRadioButton

        addToRadioGroup(radioTopToolbar, radioBottomToolbar)
        radioTopToolbar.addIllustration(view.toolbar_top_image)
        radioBottomToolbar.addIllustration(view.toolbar_bottom_image)

        val settings = view.context.components.settings
        radio = when (settings.toolbarPosition) {
            ToolbarPosition.BOTTOM -> radioBottomToolbar
            ToolbarPosition.TOP -> radioTopToolbar
        }
        radio.updateRadioValue(true)

        radioBottomToolbar.onClickListener {
            metrics.track(Event.OnboardingToolbarPosition(Position.BOTTOM))

            itemView.context.asActivity()?.recreate()
        }

        view.toolbar_bottom_image.setOnClickListener {
            metrics.track(Event.OnboardingToolbarPosition(Position.BOTTOM))

            radioBottomToolbar.performClick()
        }

        radioTopToolbar.onClickListener {
            metrics.track(Event.OnboardingToolbarPosition(Position.TOP))
            itemView.context.asActivity()?.recreate()
        }

        view.toolbar_top_image.setOnClickListener {
            metrics.track(Event.OnboardingToolbarPosition(Position.TOP))
            radioTopToolbar.performClick()
        }
    }

    companion object {
        const val LAYOUT_ID = R.layout.onboarding_toolbar_position_picker
    }
}
