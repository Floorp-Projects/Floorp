/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.onboarding

import org.mozilla.fenix.GleanMetrics.Onboarding
import org.mozilla.fenix.onboarding.view.JunoOnboardingPageType

/**
 * Abstraction responsible for recording telemetry events for JunoOnboarding.
 */
class JunoOnboardingTelemetryRecorder {

    /**
     * Records "onboarding_completed" telemetry event.
     * @param sequenceId The identifier of the onboarding sequence shown to the user.
     * @param pageType The page on which the completed event occurred.
     */
    fun onOnboardingComplete(sequenceId: String, pageType: JunoOnboardingPageType) {
        Onboarding.completed.record(
            Onboarding.CompletedExtra(
                sequenceId = sequenceId,
                sequencePosition = pageType.sequencePosition,
            ),
        )
    }

    /**
     * Records impression events for a given [JunoOnboardingPageType].
     * @param sequenceId The identifier of the onboarding sequence shown to the user.
     * @param pageType The page for which the impression occurred.
     */
    fun onImpression(sequenceId: String, pageType: JunoOnboardingPageType) {
        when (pageType) {
            JunoOnboardingPageType.DEFAULT_BROWSER -> {
                Onboarding.setToDefaultCard.record(
                    Onboarding.SetToDefaultCardExtra(
                        action = ACTION_IMPRESSION,
                        elementType = ET_ONBOARDING_CARD,
                        sequenceId = sequenceId,
                        sequencePosition = pageType.sequencePosition,
                    ),
                )
            }
            JunoOnboardingPageType.SYNC_SIGN_IN -> {
                Onboarding.signInCard.record(
                    Onboarding.SignInCardExtra(
                        action = ACTION_IMPRESSION,
                        elementType = ET_ONBOARDING_CARD,
                        sequenceId = sequenceId,
                        sequencePosition = pageType.sequencePosition,
                    ),
                )
            }
            JunoOnboardingPageType.NOTIFICATION_PERMISSION -> {
                Onboarding.turnOnNotificationsCard.record(
                    Onboarding.TurnOnNotificationsCardExtra(
                        action = ACTION_IMPRESSION,
                        elementType = ET_ONBOARDING_CARD,
                        sequenceId = sequenceId,
                        sequencePosition = pageType.sequencePosition,
                    ),
                )
            }
        }
    }

    /**
     * Records set to default click event.
     * @param sequenceId The identifier of the onboarding sequence shown to the user.
     * @param pageType The page for which the impression occurred.
     */
    fun onSetToDefaultClick(sequenceId: String, pageType: JunoOnboardingPageType) {
        Onboarding.setToDefault.record(
            Onboarding.SetToDefaultExtra(
                action = ACTION_CLICK,
                elementType = ET_PRIMARY_BUTTON,
                sequenceId = sequenceId,
                sequencePosition = pageType.sequencePosition,
            ),
        )
    }

    /**
     * Records sync sign in click event.
     * @param sequenceId The identifier of the onboarding sequence shown to the user.
     * @param pageType The page for which the impression occurred.
     */
    fun onSyncSignInClick(sequenceId: String, pageType: JunoOnboardingPageType) {
        Onboarding.signIn.record(
            Onboarding.SignInExtra(
                action = ACTION_CLICK,
                elementType = ET_PRIMARY_BUTTON,
                sequenceId = sequenceId,
                sequencePosition = pageType.sequencePosition,
            ),
        )
    }

    /**
     * Records notification permission click event.
     * @param sequenceId The identifier of the onboarding sequence shown to the user.
     * @param pageType The page for which the impression occurred.
     */
    fun onNotificationPermissionClick(sequenceId: String, pageType: JunoOnboardingPageType) {
        Onboarding.turnOnNotifications.record(
            Onboarding.TurnOnNotificationsExtra(
                action = ACTION_CLICK,
                elementType = ET_PRIMARY_BUTTON,
                sequenceId = sequenceId,
                sequencePosition = pageType.sequencePosition,
            ),
        )
    }

    /**
     * Records skip set to default click event.
     * @param sequenceId The identifier of the onboarding sequence shown to the user.
     * @param pageType The page for which the impression occurred.
     */
    fun onSkipSetToDefaultClick(
        sequenceId: String,
        pageType: JunoOnboardingPageType,
    ) {
        Onboarding.skipDefault.record(
            Onboarding.SkipDefaultExtra(
                action = ACTION_CLICK,
                elementType = ET_SECONDARY_BUTTON,
                sequenceId = sequenceId,
                sequencePosition = pageType.sequencePosition,
            ),
        )
    }

    /**
     * Records skip sign in click event.
     * @param sequenceId The identifier of the onboarding sequence shown to the user.
     * @param pageType The page for which the impression occurred.
     */
    fun onSkipSignInClick(
        sequenceId: String,
        pageType: JunoOnboardingPageType,
    ) {
        Onboarding.skipSignIn.record(
            Onboarding.SkipSignInExtra(
                action = ACTION_CLICK,
                elementType = ET_SECONDARY_BUTTON,
                sequenceId = sequenceId,
                sequencePosition = pageType.sequencePosition,
            ),
        )
    }

    /**
     * Records skip notification permission click event.
     * @param sequenceId The identifier of the onboarding sequence shown to the user.
     * @param pageType The page for which the impression occurred.
     */
    fun onSkipTurnOnNotificationsClick(
        sequenceId: String,
        pageType: JunoOnboardingPageType,
    ) {
        Onboarding.skipTurnOnNotifications.record(
            Onboarding.SkipTurnOnNotificationsExtra(
                action = ACTION_CLICK,
                elementType = ET_SECONDARY_BUTTON,
                sequenceId = sequenceId,
                sequencePosition = pageType.sequencePosition,
            ),
        )
    }

    /**
     * Records privacy policy link text click event.
     * @param sequenceId The identifier of the onboarding sequence shown to the user.
     * @param pageType The page on which the link click event occurred.
     */
    fun onPrivacyPolicyClick(sequenceId: String, pageType: JunoOnboardingPageType) {
        Onboarding.privacyPolicy.record(
            Onboarding.PrivacyPolicyExtra(
                action = ACTION_CLICK,
                elementType = ET_SECONDARY_BUTTON,
                sequenceId = sequenceId,
                sequencePosition = pageType.sequencePosition,
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
