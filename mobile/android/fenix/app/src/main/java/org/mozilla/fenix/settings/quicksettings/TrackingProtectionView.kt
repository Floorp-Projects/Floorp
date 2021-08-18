/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.quicksettings

import android.view.LayoutInflater
import android.view.ViewGroup
import androidx.annotation.VisibleForTesting
import androidx.core.view.isVisible
import org.mozilla.fenix.R
import org.mozilla.fenix.databinding.QuicksettingsTrackingProtectionBinding
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.trackingprotection.TrackingProtectionState
import org.mozilla.fenix.utils.Settings

/**
 * Contract declaring all possible user interactions with [TrackingProtectionView].
 */
interface TrackingProtectionInteractor {

    /**
     * Called whenever the tracking protection toggle for this site is toggled.
     *
     * @param isEnabled Whether or not tracking protection is enabled.
     */
    fun onTrackingProtectionToggled(isEnabled: Boolean)

    /**
     * Navigates to the tracking protection preferences. Called when a user clicks on the
     * "Details" button.
     */
    fun onDetailsClicked()
}

/**
 * MVI View that displays the tracking protection toggle and navigation to additional tracking
 * protection details.
 *
 * @param containerView [ViewGroup] in which this View will inflate itself.
 * @param interactor [TrackingProtectionInteractor] which will have delegated to all user
 * @param settings [Settings] application settings.
 * interactions.
 */
class TrackingProtectionView(
    val containerView: ViewGroup,
    val interactor: TrackingProtectionInteractor,
    val settings: Settings
) {
    private val context = containerView.context
    @VisibleForTesting
    internal val binding = QuicksettingsTrackingProtectionBinding.inflate(
        LayoutInflater.from(containerView.context),
        containerView,
        true
    )
    fun update(state: TrackingProtectionState) {
        bindTrackingProtectionInfo(state.isTrackingProtectionEnabled)
        binding.root.isVisible = settings.shouldUseTrackingProtection
        binding.trackingProtectionDetails.setOnClickListener {
            interactor.onDetailsClicked()
        }
    }

    fun updateDetailsSection(show: Boolean) {
        binding.trackingProtectionDetails.isVisible = show
    }

    private fun bindTrackingProtectionInfo(isTrackingProtectionEnabled: Boolean) {
        binding.trackingProtectionSwitch.trackingProtectionCategoryItemDescription.text =
            context.getString(if (isTrackingProtectionEnabled) R.string.etp_panel_on else R.string.etp_panel_off)
        binding.trackingProtectionSwitch.switchWidget.isChecked = isTrackingProtectionEnabled
        binding.trackingProtectionSwitch.switchWidget.jumpDrawablesToCurrentState()

        binding.trackingProtectionSwitch.switchWidget.setOnCheckedChangeListener { _, isChecked ->
            interactor.onTrackingProtectionToggled(isChecked)
        }
    }
}
