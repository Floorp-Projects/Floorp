/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.onboarding.view

import androidx.compose.runtime.Composable
import androidx.compose.runtime.ReadOnlyComposable
import androidx.compose.ui.res.stringResource
import org.mozilla.fenix.R
import org.mozilla.fenix.settings.SupportUtils

/**
 * Mapper to convert [JunoOnboardingPageType] to [OnboardingPageState] that is a param for
 * [OnboardingPage] composable.
 */
@ReadOnlyComposable
@Composable
@Suppress("LongParameterList")
internal fun mapToOnboardingPageState(
    onboardingPageType: JunoOnboardingPageType,
    scrollToNextPageOrDismiss: () -> Unit,
    onMakeFirefoxDefaultClick: () -> Unit,
    onPrivacyPolicyClick: (String) -> Unit,
    onSignInButtonClick: () -> Unit,
    onNotificationPermissionButtonClick: () -> Unit,
): OnboardingPageState = when (onboardingPageType) {
    JunoOnboardingPageType.DEFAULT_BROWSER -> defaultBrowserPageState(
        onPositiveButtonClick = {
            onMakeFirefoxDefaultClick()
            scrollToNextPageOrDismiss()
        },
        onNegativeButtonClick = scrollToNextPageOrDismiss,
        onUrlClick = onPrivacyPolicyClick,
    )
    JunoOnboardingPageType.SYNC_SIGN_IN -> syncSignInPageState(
        onPositiveButtonClick = {
            onSignInButtonClick()
            scrollToNextPageOrDismiss()
        },
        onNegativeButtonClick = scrollToNextPageOrDismiss,
    )
    JunoOnboardingPageType.NOTIFICATION_PERMISSION -> notificationPermissionPageState(
        onPositiveButtonClick = {
            onNotificationPermissionButtonClick()
            scrollToNextPageOrDismiss()
        },
        onNegativeButtonClick = scrollToNextPageOrDismiss,
    )
}

@Composable
@ReadOnlyComposable
private fun notificationPermissionPageState(
    onPositiveButtonClick: () -> Unit,
    onNegativeButtonClick: () -> Unit,
) = OnboardingPageState(
    image = R.drawable.ic_notification_permission,
    title = stringResource(
        id = R.string.juno_onboarding_enable_notifications_title,
        formatArgs = arrayOf(stringResource(R.string.app_name)),
    ),
    description = stringResource(
        id = R.string.juno_onboarding_enable_notifications_description,
        formatArgs = arrayOf(stringResource(R.string.app_name)),
    ),
    primaryButton = Action(
        text = stringResource(id = R.string.juno_onboarding_enable_notifications_positive_button),
        onClick = onPositiveButtonClick,
    ),
    secondaryButton = Action(
        text = stringResource(id = R.string.juno_onboarding_enable_notifications_negative_button),
        onClick = onNegativeButtonClick,
    ),
    onRecordImpressionEvent = {},
)

@Composable
@ReadOnlyComposable
private fun syncSignInPageState(
    onPositiveButtonClick: () -> Unit,
    onNegativeButtonClick: () -> Unit,
) = OnboardingPageState(
    image = R.drawable.ic_onboarding_sync,
    title = stringResource(id = R.string.juno_onboarding_sign_in_title),
    description = stringResource(id = R.string.juno_onboarding_sign_in_description),
    primaryButton = Action(
        text = stringResource(id = R.string.juno_onboarding_sign_in_positive_button),
        onClick = onPositiveButtonClick,
    ),
    secondaryButton = Action(
        text = stringResource(id = R.string.juno_onboarding_sign_in_negative_button),
        onClick = onNegativeButtonClick,
    ),
    onRecordImpressionEvent = {},
)

@Composable
@ReadOnlyComposable
private fun defaultBrowserPageState(
    onPositiveButtonClick: () -> Unit,
    onNegativeButtonClick: () -> Unit,
    onUrlClick: (String) -> Unit,
) = OnboardingPageState(
    image = R.drawable.ic_onboarding_welcome,
    title = stringResource(
        id = R.string.juno_onboarding_default_browser_title,
        formatArgs = arrayOf(stringResource(R.string.app_name)),
    ),
    description = stringResource(
        id = R.string.juno_onboarding_default_browser_description,
        formatArgs = arrayOf(
            stringResource(R.string.firefox),
            stringResource(R.string.juno_onboarding_default_browser_description_link_text),
        ),
    ),
    linkTextState = LinkTextState(
        text = stringResource(id = R.string.juno_onboarding_default_browser_description_link_text),
        url = SupportUtils.getMozillaPageUrl(SupportUtils.MozillaPage.PRIVATE_NOTICE),
        onClick = onUrlClick,
    ),
    primaryButton = Action(
        text = stringResource(id = R.string.juno_onboarding_default_browser_positive_button),
        onClick = onPositiveButtonClick,
    ),
    secondaryButton = Action(
        text = stringResource(id = R.string.juno_onboarding_default_browser_negative_button),
        onClick = onNegativeButtonClick,
    ),
    onRecordImpressionEvent = {},
)
