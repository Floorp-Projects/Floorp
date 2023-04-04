/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.onboarding.view

/**
 * Model for different types of Onboarding Pages.
 * @param sequencePosition Position of the page in the onboarding flow, used in telemetry.
 * @param id Identifier for the page, used in telemetry.
 */
enum class JunoOnboardingPageType(
    val sequencePosition: String,
    val id: String,
) {
    DEFAULT_BROWSER(
        sequencePosition = "1",
        id = "default",
    ),
    SYNC_SIGN_IN(
        sequencePosition = "2",
        id = "sync",
    ),
    NOTIFICATION_PERMISSION(
        sequencePosition = "3",
        id = "notification",
    ),
}

/**
 * Helper function for telemetry that maps List<JunoOnboardingPageType> to a string of page names
 * separated by an underscore.
 * e.g. [DEFAULT_BROWSER, SYNC_SIGN_IN] will be mapped to "default_sync".
 */
fun List<JunoOnboardingPageType>.telemetrySequenceId(): String =
    joinToString(separator = "_") { it.id }
