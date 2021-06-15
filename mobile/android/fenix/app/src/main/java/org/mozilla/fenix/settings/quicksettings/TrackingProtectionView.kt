/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.quicksettings

import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import kotlinx.android.extensions.LayoutContainer
import kotlinx.android.synthetic.main.component_tracking_protection_panel.trackingProtectionSwitch
import kotlinx.android.synthetic.main.quicksettings_tracking_protection.*
import kotlinx.android.synthetic.main.switch_with_description.view.*
import org.mozilla.fenix.R
import org.mozilla.fenix.trackingprotection.TrackingProtectionState

/**
 * Contract declaring all possible user interactions with [TrackingProtectionView]
 */
interface TrackingProtectionInteractor {

    /**
     * Called whenever the tracking protection toggle for this site is toggled
     *
     * @param isEnabled new status of session tracking protection
     */
    fun onTrackingProtectionToggled(isEnabled: Boolean)

    /**
     * Navigates to the tracking protection preferences. Called when a user clicks on the
     * "Blocked items" button.
     */
    fun onBlockedItemsClicked()
}

/**
 * TODO
 *
 * @param containerView [ViewGroup] in which this View will inflate itself.
 * @param interactor [TrackingProtectionInteractor] which will have delegated to all user interactions.
 */
class TrackingProtectionView(
    override val containerView: ViewGroup,
    val interactor: TrackingProtectionInteractor
) : LayoutContainer {

    private val context = containerView.context
    val view: View = LayoutInflater.from(context)
        .inflate(R.layout.quicksettings_tracking_protection, containerView, true)

    fun update(state: TrackingProtectionState) {
        bindTrackingProtectionInfo(state.isTrackingProtectionEnabled)

        trackingProtectionBlockedItems.setOnClickListener {
            interactor.onBlockedItemsClicked()
        }
    }

    private fun bindTrackingProtectionInfo(isTrackingProtectionEnabled: Boolean) {
        trackingProtectionSwitch.switch_widget.isChecked = isTrackingProtectionEnabled
        trackingProtectionSwitch.switch_widget.jumpDrawablesToCurrentState()

        trackingProtectionSwitch.switch_widget.setOnCheckedChangeListener { _, isChecked ->
            interactor.onTrackingProtectionToggled(isChecked)
        }
    }
}
