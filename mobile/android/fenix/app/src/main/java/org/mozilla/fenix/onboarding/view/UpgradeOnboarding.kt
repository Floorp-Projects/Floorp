/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.onboarding.view

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.navigationBarsPadding
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.statusBarsPadding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import mozilla.telemetry.glean.private.NoExtras
import org.mozilla.fenix.R
import org.mozilla.fenix.compose.annotation.LightDarkPreview
import org.mozilla.fenix.theme.FirefoxTheme
import org.mozilla.fenix.GleanMetrics.Onboarding as OnboardingMetrics

/**
 * Enum that represents the onboarding screen that is displayed.
 */
private enum class UpgradeOnboardingState {
    Welcome,
    SyncSignIn,
}

/**
 * A screen for displaying a welcome and sync sign in onboarding.
 *
 * @param isSyncSignIn Whether or not the user is signed into their Firefox Sync account.
 * @param onDismiss Invoked when the user clicks on the close or "Skip" button.
 * @param onSignInButtonClick Invoked when the user clicks on the "Sign In" button
 */
@Composable
fun UpgradeOnboarding(
    isSyncSignIn: Boolean,
    onDismiss: () -> Unit,
    onSignInButtonClick: () -> Unit,
) {
    var onboardingState by remember { mutableStateOf(UpgradeOnboardingState.Welcome) }

    Column(
        modifier = Modifier
            .background(FirefoxTheme.colors.layer1)
            .fillMaxSize()
            .padding(bottom = 32.dp)
            .statusBarsPadding()
            .navigationBarsPadding(),
    ) {
        OnboardingPage(
            pageState = when (onboardingState) {
                UpgradeOnboardingState.Welcome -> OnboardingPageState(
                    image = R.drawable.ic_onboarding_welcome,
                    title = stringResource(id = R.string.onboarding_home_welcome_title_2),
                    description = stringResource(id = R.string.onboarding_home_welcome_description),
                    primaryButton = Action(
                        text = stringResource(id = R.string.onboarding_home_get_started_button),
                        onClick = {
                            OnboardingMetrics.welcomeGetStartedClicked.record(NoExtras())
                            if (isSyncSignIn) {
                                onDismiss()
                            } else {
                                onboardingState = UpgradeOnboardingState.SyncSignIn
                            }
                        },
                    ),
                    onRecordImpressionEvent = {
                        OnboardingMetrics.welcomeCardImpression.record(NoExtras())
                    },
                )
                UpgradeOnboardingState.SyncSignIn -> OnboardingPageState(
                    image = R.drawable.ic_onboarding_sync,
                    title = stringResource(id = R.string.onboarding_home_sync_title_3),
                    description = stringResource(id = R.string.onboarding_home_sync_description),
                    primaryButton = Action(
                        text = stringResource(id = R.string.onboarding_home_sign_in_button),
                        onClick = {
                            OnboardingMetrics.syncSignInClicked.record(NoExtras())
                            onSignInButtonClick()
                        },
                    ),
                    secondaryButton = Action(
                        text = stringResource(id = R.string.onboarding_home_skip_button),
                        onClick = {
                            OnboardingMetrics.syncSkipClicked.record(NoExtras())
                            onDismiss()
                        },
                    ),
                    onRecordImpressionEvent = {
                        OnboardingMetrics.syncCardImpression.record(NoExtras())
                    },
                )
            },
            onDismiss = {
                when (onboardingState) {
                    UpgradeOnboardingState.Welcome -> OnboardingMetrics.welcomeCloseClicked.record(NoExtras())
                    UpgradeOnboardingState.SyncSignIn -> OnboardingMetrics.syncCloseClicked.record(NoExtras())
                }
                onDismiss()
            },
            modifier = Modifier.weight(1f),
        )

        if (isSyncSignIn) {
            Spacer(modifier = Modifier.height(6.dp))
        } else {
            Indicators(onboardingState = onboardingState)
        }
    }
}

@Composable
private fun Indicators(
    onboardingState: UpgradeOnboardingState,
) {
    Row(
        modifier = Modifier.fillMaxWidth(),
        horizontalArrangement = Arrangement.Center,
    ) {
        Indicator(
            color = if (onboardingState == UpgradeOnboardingState.Welcome) {
                FirefoxTheme.colors.indicatorActive
            } else {
                FirefoxTheme.colors.indicatorInactive
            },
        )

        Spacer(modifier = Modifier.width(8.dp))

        Indicator(
            color = if (onboardingState == UpgradeOnboardingState.SyncSignIn) {
                FirefoxTheme.colors.indicatorActive
            } else {
                FirefoxTheme.colors.indicatorInactive
            },
        )
    }
}

@Composable
private fun Indicator(color: Color) {
    Box(
        modifier = Modifier
            .size(6.dp)
            .clip(CircleShape)
            .background(color),
    )
}

@Composable
@LightDarkPreview
private fun OnboardingPreview() {
    FirefoxTheme {
        UpgradeOnboarding(
            isSyncSignIn = false,
            onDismiss = {},
            onSignInButtonClick = {},
        )
    }
}
