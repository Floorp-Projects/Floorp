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
 * @param primaryButton [Action] action for the primary button.
 * @param secondaryButton [Action] action for the secondary button.
 * @param onRecordImpressionEvent Callback for recording impression event.
 */
data class OnboardingPageState(
    @DrawableRes val image: Int,
    val title: String,
    val description: String,
    val primaryButton: Action,
    val secondaryButton: Action? = null,
    val onRecordImpressionEvent: () -> Unit,
)

/**
 * Model containing text and action for a button.
 */
data class Action(
    val text: String,
    val onClick: () -> Unit,
)
