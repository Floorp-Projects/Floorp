/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.sessioncontrol.viewholders.onboarding

import android.view.View
import androidx.recyclerview.widget.RecyclerView
import kotlinx.android.synthetic.main.onboarding_toolbar_position_picker.view.toolbar_bottom_image
import kotlinx.android.synthetic.main.onboarding_toolbar_position_picker.view.toolbar_bottom_radio_button
import kotlinx.android.synthetic.main.onboarding_toolbar_position_picker.view.toolbar_top_image
import kotlinx.android.synthetic.main.onboarding_toolbar_position_picker.view.toolbar_top_radio_button
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.asActivity
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.onboarding.OnboardingRadioButton

class OnboardingToolbarPositionPickerViewHolder(view: View) : RecyclerView.ViewHolder(view) {

    init {
        val radioTopToolbar = view.toolbar_top_radio_button
        val radioBottomToolbar = view.toolbar_bottom_radio_button
        val radio: OnboardingRadioButton

        radioTopToolbar.addToRadioGroup(radioBottomToolbar)
        radioBottomToolbar.addToRadioGroup(radioTopToolbar)

        if (view.context.settings().shouldUseBottomToolbar) {
            radio = radioBottomToolbar
            setBottomIllustrationSelected()
        } else {
            radio = radioTopToolbar
            setTopIllustrationSelected()
        }
        radio.isChecked = true

        radioBottomToolbar.onClickListener {
            setBottomIllustrationSelected()
            itemView.context.asActivity()?.recreate()
        }

        view.toolbar_bottom_image.setOnClickListener {
            radioBottomToolbar.performClick()
        }

        radioTopToolbar.onClickListener {
            setTopIllustrationSelected()
            itemView.context.asActivity()?.recreate()
        }

        view.toolbar_top_image.setOnClickListener {
            radioTopToolbar.performClick()
        }
    }

    private fun setTopIllustrationSelected() {
        itemView.toolbar_top_image.isSelected = true
        itemView.toolbar_bottom_image.isSelected = false
    }

    private fun setBottomIllustrationSelected() {
        itemView.toolbar_top_image.isSelected = false
        itemView.toolbar_bottom_image.isSelected = true
    }

    companion object {
        const val LAYOUT_ID = R.layout.onboarding_toolbar_position_picker
    }
}
