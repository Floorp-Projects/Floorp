/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.onboarding.view

import androidx.annotation.DrawableRes
import org.mozilla.fenix.compose.LinkTextState

/**
 * Model containing data for [OnboardingPage].
 *
 * @property imageRes [DrawableRes] displayed on the page.
 * @property title [String] title of the page.
 * @property description [String] description of the page.
 * @property linkTextState [LinkTextState] part of description text with a link.
 * @property primaryButton [Action] action for the primary button.
 * @property secondaryButton [Action] action for the secondary button.
 * @property onRecordImpressionEvent Callback for recording impression event.
 */
data class OnboardingPageState(
    @DrawableRes val imageRes: Int,
    val title: String,
    val description: String,
    val linkTextState: LinkTextState? = null,
    val primaryButton: Action,
    val secondaryButton: Action? = null,
    val onRecordImpressionEvent: () -> Unit = {},
)

/**
 * Model containing text and action for a button.
 */
data class Action(
    val text: String,
    val onClick: () -> Unit,
)
