/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.onboarding.view

import org.mozilla.fenix.compose.LinkTextState
import org.mozilla.fenix.nimbus.OnboardingCardData
import org.mozilla.fenix.nimbus.OnboardingCardType
import org.mozilla.fenix.settings.SupportUtils

/**
 * Returns a list of all the required Nimbus 'cards' that have been converted to [OnboardingPageUiData].
 */
internal fun Collection<OnboardingCardData>.toPageUiData(
    showNotificationPage: Boolean,
    showAddWidgetPage: Boolean,
): List<OnboardingPageUiData> =
    filter {
        when (it.cardType) {
            OnboardingCardType.NOTIFICATION_PERMISSION -> {
                it.enabled && showNotificationPage
            }
            OnboardingCardType.ADD_SEARCH_WIDGET -> {
                it.enabled && showAddWidgetPage
            }
            else -> {
                it.enabled
            }
        }
    }.sortedBy { it.ordering }
        .map { it.toPageUiData() }

private fun OnboardingCardData.toPageUiData() = OnboardingPageUiData(
    type = cardType.toPageUiDataType(),
    imageRes = imageRes.resourceId,
    title = title,
    description = body,
    linkText = linkText,
    primaryButtonLabel = primaryButtonLabel,
    secondaryButtonLabel = secondaryButtonLabel,
)

private fun OnboardingCardType.toPageUiDataType() = when (this) {
    OnboardingCardType.DEFAULT_BROWSER -> OnboardingPageUiData.Type.DEFAULT_BROWSER
    OnboardingCardType.SYNC_SIGN_IN -> OnboardingPageUiData.Type.SYNC_SIGN_IN
    OnboardingCardType.NOTIFICATION_PERMISSION -> OnboardingPageUiData.Type.NOTIFICATION_PERMISSION
    OnboardingCardType.ADD_SEARCH_WIDGET -> OnboardingPageUiData.Type.ADD_SEARCH_WIDGET
}

/**
 * Mapper to convert [OnboardingPageUiData] to [OnboardingPageState] that is a param for
 * [OnboardingPage] composable.
 */
@Suppress("LongParameterList")
internal fun mapToOnboardingPageState(
    onboardingPageUiData: OnboardingPageUiData,
    onMakeFirefoxDefaultClick: () -> Unit,
    onMakeFirefoxDefaultSkipClick: () -> Unit,
    onPrivacyPolicyClick: (String) -> Unit,
    onSignInButtonClick: () -> Unit,
    onSignInSkipClick: () -> Unit,
    onNotificationPermissionButtonClick: () -> Unit,
    onNotificationPermissionSkipClick: () -> Unit,
    onAddFirefoxWidgetClick: () -> Unit,
    onAddFirefoxWidgetSkipClick: () -> Unit,
): OnboardingPageState = when (onboardingPageUiData.type) {
    OnboardingPageUiData.Type.DEFAULT_BROWSER -> createOnboardingPageState(
        onboardingPageUiData = onboardingPageUiData,
        onPositiveButtonClick = onMakeFirefoxDefaultClick,
        onNegativeButtonClick = onMakeFirefoxDefaultSkipClick,
        onUrlClick = onPrivacyPolicyClick,
    )

    OnboardingPageUiData.Type.ADD_SEARCH_WIDGET -> createOnboardingPageState(
        onboardingPageUiData = onboardingPageUiData,
        onPositiveButtonClick = onAddFirefoxWidgetClick,
        onNegativeButtonClick = onAddFirefoxWidgetSkipClick,
        onUrlClick = onPrivacyPolicyClick,
    )

    OnboardingPageUiData.Type.SYNC_SIGN_IN -> createOnboardingPageState(
        onboardingPageUiData = onboardingPageUiData,
        onPositiveButtonClick = onSignInButtonClick,
        onNegativeButtonClick = onSignInSkipClick,
    )

    OnboardingPageUiData.Type.NOTIFICATION_PERMISSION -> createOnboardingPageState(
        onboardingPageUiData = onboardingPageUiData,
        onPositiveButtonClick = onNotificationPermissionButtonClick,
        onNegativeButtonClick = onNotificationPermissionSkipClick,
    )
}

private fun createOnboardingPageState(
    onboardingPageUiData: OnboardingPageUiData,
    onPositiveButtonClick: () -> Unit,
    onNegativeButtonClick: () -> Unit,
    onUrlClick: (String) -> Unit = {},
): OnboardingPageState = OnboardingPageState(
    imageRes = onboardingPageUiData.imageRes,
    title = onboardingPageUiData.title,
    description = onboardingPageUiData.description,
    linkTextState = onboardingPageUiData.linkText?.let {
        LinkTextState(
            text = it,
            url = SupportUtils.getMozillaPageUrl(SupportUtils.MozillaPage.PRIVATE_NOTICE),
            onClick = onUrlClick,
        )
    },
    primaryButton = Action(onboardingPageUiData.primaryButtonLabel, onPositiveButtonClick),
    secondaryButton = Action(onboardingPageUiData.secondaryButtonLabel, onNegativeButtonClick),
)
