/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.onboarding.view

import androidx.compose.foundation.layout.navigationBarsPadding
import androidx.compose.foundation.layout.statusBarsPadding
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import mozilla.components.service.glean.private.NoExtras
import org.mozilla.fenix.GleanMetrics.Onboarding
import org.mozilla.fenix.R
import org.mozilla.fenix.compose.annotation.LightDarkPreview
import org.mozilla.fenix.theme.FirefoxTheme

/**
 * A screen for displaying notification pre permission prompt.
 *
 * @param onDismiss Invoked when the user clicks on the close or the negative button.
 * @param grantNotificationPermission Invoked when the user clicks on the positive button.
 */
@Composable
fun NotificationPermissionDialogScreen(
    onDismiss: () -> Unit,
    grantNotificationPermission: () -> Unit,
) {
    OnboardingPage(
        pageState = OnboardingPageState(
            image = R.drawable.ic_notification_permission,
            title = stringResource(
                id = R.string.onboarding_home_enable_notifications_title,
                formatArgs = arrayOf(stringResource(R.string.app_name)),
            ),
            description = stringResource(
                id = R.string.onboarding_home_enable_notifications_description,
                formatArgs = arrayOf(stringResource(R.string.app_name)),
            ),
            primaryButton = Action(
                text = stringResource(id = R.string.onboarding_home_enable_notifications_positive_button),
                onClick = {
                    grantNotificationPermission()
                    Onboarding.notifPppPositiveBtnClick.record(NoExtras())
                },
            ),
            secondaryButton = Action(
                text = stringResource(id = R.string.onboarding_home_enable_notifications_negative_button),
                onClick = {
                    onDismiss()
                    Onboarding.notifPppNegativeBtnClick.record(NoExtras())
                },
            ),
            onRecordImpressionEvent = { Onboarding.notifPppImpression.record(NoExtras()) },
        ),
        onDismiss = {
            onDismiss()
            Onboarding.notifPppCloseClick.record(NoExtras())
        },
        modifier = Modifier
            .statusBarsPadding()
            .navigationBarsPadding(),
    )
}

@LightDarkPreview
@Composable
private fun NotificationPermissionScreenPreview() {
    FirefoxTheme {
        NotificationPermissionDialogScreen(
            grantNotificationPermission = {},
            onDismiss = { },
        )
    }
}
