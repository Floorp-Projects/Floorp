/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.onboarding

import org.mozilla.fenix.GleanMetrics.Onboarding
import org.mozilla.fenix.onboarding.view.OnboardingPageUiData

/**
 * Abstraction responsible for recording telemetry events for JunoOnboarding.
 */
class JunoOnboardingTelemetryRecorder {

    /**
     * Records "onboarding_completed" telemetry event.
     * @param sequenceId The identifier of the onboarding sequence shown to the user.
     * @param sequencePosition The sequence position of the page on which the completed event occurred.
     */
    fun onOnboardingComplete(sequenceId: String, sequencePosition: String) {
        Onboarding.completed.record(
            Onboarding.CompletedExtra(
                sequenceId = sequenceId,
                sequencePosition = sequencePosition,
            ),
        )
    }

    /**
     * Records impression events for a given [OnboardingPageUiData.Type].
     * @param sequenceId The identifier of the onboarding sequence shown to the user.
     * @param pageType The page type for which the impression occurred.
     * @param sequencePosition The sequence position of the page for which the impression occurred.
     */
    fun onImpression(
        sequenceId: String,
        pageType: OnboardingPageUiData.Type,
        sequencePosition: String,
    ) {
        when (pageType) {
            OnboardingPageUiData.Type.DEFAULT_BROWSER -> {
                Onboarding.setToDefaultCard.record(
                    Onboarding.SetToDefaultCardExtra(
                        action = ACTION_IMPRESSION,
                        elementType = ET_ONBOARDING_CARD,
                        sequenceId = sequenceId,
                        sequencePosition = sequencePosition,
                    ),
                )
            }

            OnboardingPageUiData.Type.SYNC_SIGN_IN -> {
                Onboarding.signInCard.record(
                    Onboarding.SignInCardExtra(
                        action = ACTION_IMPRESSION,
                        elementType = ET_ONBOARDING_CARD,
                        sequenceId = sequenceId,
                        sequencePosition = sequencePosition,
                    ),
                )
            }

            OnboardingPageUiData.Type.NOTIFICATION_PERMISSION -> {
                Onboarding.turnOnNotificationsCard.record(
                    Onboarding.TurnOnNotificationsCardExtra(
                        action = ACTION_IMPRESSION,
                        elementType = ET_ONBOARDING_CARD,
                        sequenceId = sequenceId,
                        sequencePosition = sequencePosition,
                    ),
                )
            }
        }
    }

    /**
     * Records set to default click event.
     * @param sequenceId The identifier of the onboarding sequence shown to the user.
     * @param sequencePosition The sequence position of the page for which the impression occurred.
     */
    fun onSetToDefaultClick(sequenceId: String, sequencePosition: String) {
        Onboarding.setToDefault.record(
            Onboarding.SetToDefaultExtra(
                action = ACTION_CLICK,
                elementType = ET_PRIMARY_BUTTON,
                sequenceId = sequenceId,
                sequencePosition = sequencePosition,
            ),
        )
    }

    /**
     * Records sync sign in click event.
     * @param sequenceId The identifier of the onboarding sequence shown to the user.
     * @param sequencePosition The sequence position of the page for which the impression occurred.
     */
    fun onSyncSignInClick(sequenceId: String, sequencePosition: String) {
        Onboarding.signIn.record(
            Onboarding.SignInExtra(
                action = ACTION_CLICK,
                elementType = ET_PRIMARY_BUTTON,
                sequenceId = sequenceId,
                sequencePosition = sequencePosition,
            ),
        )
    }

    /**
     * Records notification permission click event.
     * @param sequenceId The identifier of the onboarding sequence shown to the user.
     * @param sequencePosition The sequence position of the page for which the impression occurred.
     */
    fun onNotificationPermissionClick(sequenceId: String, sequencePosition: String) {
        Onboarding.turnOnNotifications.record(
            Onboarding.TurnOnNotificationsExtra(
                action = ACTION_CLICK,
                elementType = ET_PRIMARY_BUTTON,
                sequenceId = sequenceId,
                sequencePosition = sequencePosition,
            ),
        )
    }

    /**
     * Records skip set to default click event.
     * @param sequenceId The identifier of the onboarding sequence shown to the user.
     * @param sequencePosition The sequence position of the page for which the impression occurred.
     */
    fun onSkipSetToDefaultClick(sequenceId: String, sequencePosition: String) {
        Onboarding.skipDefault.record(
            Onboarding.SkipDefaultExtra(
                action = ACTION_CLICK,
                elementType = ET_SECONDARY_BUTTON,
                sequenceId = sequenceId,
                sequencePosition = sequencePosition,
            ),
        )
    }

    /**
     * Records skip sign in click event.
     * @param sequenceId The identifier of the onboarding sequence shown to the user.
     * @param sequencePosition The sequence position of the page for which the impression occurred.
     */
    fun onSkipSignInClick(sequenceId: String, sequencePosition: String) {
        Onboarding.skipSignIn.record(
            Onboarding.SkipSignInExtra(
                action = ACTION_CLICK,
                elementType = ET_SECONDARY_BUTTON,
                sequenceId = sequenceId,
                sequencePosition = sequencePosition,
            ),
        )
    }

    /**
     * Records skip notification permission click event.
     * @param sequenceId The identifier of the onboarding sequence shown to the user.
     * @param sequencePosition The sequence position of the page for which the impression occurred.
     */
    fun onSkipTurnOnNotificationsClick(sequenceId: String, sequencePosition: String) {
        Onboarding.skipTurnOnNotifications.record(
            Onboarding.SkipTurnOnNotificationsExtra(
                action = ACTION_CLICK,
                elementType = ET_SECONDARY_BUTTON,
                sequenceId = sequenceId,
                sequencePosition = sequencePosition,
            ),
        )
    }

    /**
     * Records privacy policy link text click event.
     * @param sequenceId The identifier of the onboarding sequence shown to the user.
     * @param sequencePosition The sequence position of the page on which the link click event occurred.
     */
    fun onPrivacyPolicyClick(sequenceId: String, sequencePosition: String) {
        Onboarding.privacyPolicy.record(
            Onboarding.PrivacyPolicyExtra(
                action = ACTION_CLICK,
                elementType = ET_SECONDARY_BUTTON,
                sequenceId = sequenceId,
                sequencePosition = sequencePosition,
            ),
        )
    }

    companion object {
        private const val ACTION_IMPRESSION = "impression"
        private const val ACTION_CLICK = "click"
        private const val ET_ONBOARDING_CARD = "onboarding_card"
        private const val ET_PRIMARY_BUTTON = "primary_button"
        private const val ET_SECONDARY_BUTTON = "secondary_button"
    }
}
