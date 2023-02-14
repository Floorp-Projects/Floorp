/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.onboarding.view

import androidx.annotation.DrawableRes

/**
 * Model containing data for [OnboardingPage].
 *
 * @param image [DrawableRes] displayed on the page.
 * @param title [String] title of the page.
 * @param description [String] description of the page.
 * @param primaryButtonText [String] text for the primary button.
 * @param secondaryButtonText [String] text for the secondary button.
 * @param onRecordImpressionEvent Callback for recording impression event.
 */
data class OnboardingPageState(
    @DrawableRes val image: Int,
    val title: String,
    val description: String,
    val primaryButtonText: String,
    val secondaryButtonText: String? = null,
    val onRecordImpressionEvent: () -> Unit,
)
