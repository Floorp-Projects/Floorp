/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.onboarding.view

import androidx.annotation.DrawableRes
import androidx.annotation.StringRes
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.Icon
import androidx.compose.material.IconButton
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import com.google.accompanist.insets.navigationBarsPadding
import com.google.accompanist.insets.statusBarsPadding
import org.mozilla.fenix.R
import org.mozilla.fenix.compose.button.PrimaryButton
import org.mozilla.fenix.compose.button.SecondaryButton
import org.mozilla.fenix.theme.FirefoxTheme

/**
 * Model containing data for the [NotificationPermissionPage].
 *
 * @param image [DrawableRes] displayed on the page.
 * @param title [StringRes] of the permission headline text.
 * @param description [StringRes] of the permission body text.
 * @param primaryButtonText [StringRes] of the primary button text.
 * @param secondaryButtonText [StringRes] of the secondary button text.
 * @param onRecordImpressionEvent Callback for recording impression event.
 */
private data class NotificationPermissionPageState(
    @DrawableRes val image: Int,
    @StringRes val title: Int,
    @StringRes val description: Int,
    @StringRes val primaryButtonText: Int,
    @StringRes val secondaryButtonText: Int? = null,
    val onRecordImpressionEvent: () -> Unit,
)

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
    NotificationPermissionContent(
        notificationPermissionPageState = NotificationPageState,
        onDismiss = onDismiss,
        onPrimaryButtonClick = grantNotificationPermission,
        onSecondaryButtonClick = onDismiss,
    )
}

@Composable
private fun NotificationPermissionContent(
    notificationPermissionPageState: NotificationPermissionPageState,
    onDismiss: () -> Unit,
    onPrimaryButtonClick: () -> Unit,
    onSecondaryButtonClick: () -> Unit,
    modifier: Modifier = Modifier,
) {
    BoxWithConstraints(Modifier.fillMaxSize()) {
        val boxWithConstraintsScope = this
        Column(
            modifier = modifier
                .background(FirefoxTheme.colors.layer1)
                .fillMaxSize()
                .padding(bottom = 32.dp)
                .statusBarsPadding()
                .navigationBarsPadding(),
            horizontalAlignment = Alignment.CenterHorizontally,
        ) {
            IconButton(
                onClick = onDismiss,
                modifier = Modifier.align(Alignment.End),
            ) {
                Icon(
                    painter = painterResource(id = R.drawable.mozac_ic_close),
                    contentDescription = stringResource(R.string.content_description_close_button),
                    tint = FirefoxTheme.colors.iconPrimary,
                )
            }

            NotificationPermissionPage(
                pageState = notificationPermissionPageState,
                onPrimaryButtonClick = onPrimaryButtonClick,
                onSecondaryButtonClick = onSecondaryButtonClick,
                modifier = Modifier
                    .weight(1f)
                    .fillMaxWidth()
                    .padding(horizontal = 16.dp)
                    .verticalScroll(rememberScrollState()),
                imageModifier = Modifier
                    .height(boxWithConstraintsScope.maxHeight.times(IMAGE_HEIGHT_RATIO)),
            )
        }
    }
}

/**
 * A page for displaying Notification Permission Content.
 *
 * @param pageState The page content that's displayed.
 * @param onPrimaryButtonClick Invoked when the user clicks the primary button.
 * @param onSecondaryButtonClick Invoked when the user clicks the secondary button.
 * @param modifier The modifier to be applied to the Composable.
 */
@Composable
private fun NotificationPermissionPage(
    pageState: NotificationPermissionPageState,
    onPrimaryButtonClick: () -> Unit,
    onSecondaryButtonClick: () -> Unit,
    modifier: Modifier = Modifier,
    imageModifier: Modifier = Modifier,
) {
    Column(
        modifier = modifier,
        horizontalAlignment = Alignment.CenterHorizontally,
        verticalArrangement = Arrangement.SpaceEvenly,
    ) {
        Image(
            painter = painterResource(id = pageState.image),
            contentDescription = null,
            modifier = imageModifier,
        )

        Column(
            modifier = Modifier.fillMaxWidth(),
            horizontalAlignment = Alignment.CenterHorizontally,
        ) {
            Text(
                text = stringResource(
                    id = pageState.title,
                    formatArgs = arrayOf(stringResource(R.string.app_name)),
                ),
                color = FirefoxTheme.colors.textPrimary,
                textAlign = TextAlign.Center,
                style = FirefoxTheme.typography.headline5,
            )

            Spacer(modifier = Modifier.height(16.dp))

            Text(
                text = stringResource(
                    id = pageState.description,
                    formatArgs = arrayOf(stringResource(R.string.app_name)),
                ),
                color = FirefoxTheme.colors.textSecondary,
                textAlign = TextAlign.Center,
                style = FirefoxTheme.typography.body2,
            )
        }

        Column(
            horizontalAlignment = Alignment.CenterHorizontally,
            modifier = Modifier.padding(top = 16.dp),
        ) {
            PrimaryButton(
                text = stringResource(id = pageState.primaryButtonText),
                onClick = onPrimaryButtonClick,
            )

            if (pageState.secondaryButtonText != null) {
                Spacer(modifier = Modifier.height(8.dp))
                SecondaryButton(
                    text = stringResource(id = pageState.secondaryButtonText),
                    onClick = onSecondaryButtonClick,
                )
            }
        }
    }

    LaunchedEffect(pageState) {
        pageState.onRecordImpressionEvent()
    }
}

private val NotificationPageState = NotificationPermissionPageState(
    image = R.drawable.ic_notification_permission,
    title = R.string.onboarding_home_enable_notifications_title,
    description = R.string.onboarding_home_enable_notifications_description,
    primaryButtonText = R.string.onboarding_home_enable_notifications_positive_button,
    secondaryButtonText = R.string.onboarding_home_enable_notifications_negative_button,
    onRecordImpressionEvent = {},
)

private const val IMAGE_HEIGHT_RATIO = 0.4f

@Preview
@Composable
private fun NotificationPermissionScreenPreview() {
    FirefoxTheme {
        NotificationPermissionDialogScreen(
            grantNotificationPermission = {},
            onDismiss = { },
        )
    }
}
