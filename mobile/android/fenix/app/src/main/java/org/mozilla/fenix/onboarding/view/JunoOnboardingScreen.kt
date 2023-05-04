/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:OptIn(ExperimentalFoundationApi::class)

package org.mozilla.fenix.onboarding.view

import androidx.activity.compose.BackHandler
import androidx.compose.foundation.ExperimentalFoundationApi
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.navigationBarsPadding
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.statusBarsPadding
import androidx.compose.foundation.pager.HorizontalPager
import androidx.compose.foundation.pager.PagerState
import androidx.compose.foundation.pager.rememberPagerState
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.State
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.snapshotFlow
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.input.nestedscroll.NestedScrollConnection
import androidx.compose.ui.input.nestedscroll.NestedScrollSource
import androidx.compose.ui.input.nestedscroll.nestedScroll
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import kotlinx.coroutines.launch
import mozilla.components.lib.state.ext.observeAsComposableState
import org.mozilla.fenix.R
import org.mozilla.fenix.components.components
import org.mozilla.fenix.compose.PagerIndicator
import org.mozilla.fenix.compose.annotation.LightDarkPreview
import org.mozilla.fenix.theme.FirefoxTheme

/**
 * A screen for displaying juno onboarding.
 *
 * @param pagesToDisplay List of pages to be displayed in onboarding pager ui.
 * @param onMakeFirefoxDefaultClick Invoked when positive button on default browser page is clicked.
 * @param onSkipDefaultClick Invoked when negative button on default browser page is clicked.
 * @param onPrivacyPolicyClick Invoked when the privacy policy link text is clicked.
 * @param onSignInButtonClick Invoked when the positive button on the sign in page is clicked.
 * @param onSkipSignInClick Invoked when the negative button on the sign in page is clicked.
 * @param onNotificationPermissionButtonClick Invoked when positive button on notification page is
 * clicked.
 * @param onSkipNotificationClick Invoked when negative button on notification page is clicked.
 * @param onFinish Invoked when the onboarding is completed.
 * @param onImpression Invoked when a page in the pager is displayed.
 */
@Composable
@Suppress("LongParameterList")
fun JunoOnboardingScreen(
    pagesToDisplay: List<OnboardingPageUiData>,
    onMakeFirefoxDefaultClick: () -> Unit,
    onSkipDefaultClick: () -> Unit,
    onPrivacyPolicyClick: (url: String) -> Unit,
    onSignInButtonClick: () -> Unit,
    onSkipSignInClick: () -> Unit,
    onNotificationPermissionButtonClick: () -> Unit,
    onSkipNotificationClick: () -> Unit,
    onFinish: (pageType: OnboardingPageUiData) -> Unit,
    onImpression: (pageType: OnboardingPageUiData) -> Unit,
) {
    val coroutineScope = rememberCoroutineScope()
    val pagerState = rememberPagerState()
    val isSignedIn: State<Boolean?> = components.backgroundServices.syncStore
        .observeAsComposableState { it.account != null }

    BackHandler(enabled = pagerState.currentPage > 0) {
        coroutineScope.launch {
            pagerState.animateScrollToPage(pagerState.currentPage - 1)
        }
    }

    val scrollToNextPageOrDismiss: () -> Unit = {
        if (pagerState.currentPage == pagesToDisplay.lastIndex) {
            onFinish(pagesToDisplay[pagerState.currentPage])
        } else {
            coroutineScope.launch {
                pagerState.animateScrollToPage(pagerState.currentPage + 1)
            }
        }
    }

    LaunchedEffect(isSignedIn.value) {
        if (isSignedIn.value == true) {
            scrollToNextPageOrDismiss()
        }
    }

    LaunchedEffect(pagerState) {
        snapshotFlow { pagerState.currentPage }.collect { page ->
            onImpression(pagesToDisplay[page])
        }
    }

    JunoOnboardingContent(
        pagesToDisplay = pagesToDisplay,
        pagerState = pagerState,
        onMakeFirefoxDefaultClick = {
            scrollToNextPageOrDismiss()
            onMakeFirefoxDefaultClick()
        },
        onMakeFirefoxDefaultSkipClick = {
            scrollToNextPageOrDismiss()
            onSkipDefaultClick()
        },
        onPrivacyPolicyClick = {
            onPrivacyPolicyClick(it)
        },
        onSignInButtonClick = {
            onSignInButtonClick()
            scrollToNextPageOrDismiss()
        },
        onSignInSkipClick = {
            scrollToNextPageOrDismiss()
            onSkipSignInClick()
        },
        onNotificationPermissionButtonClick = {
            scrollToNextPageOrDismiss()
            onNotificationPermissionButtonClick()
        },
        onNotificationPermissionSkipClick = {
            scrollToNextPageOrDismiss()
            onSkipNotificationClick()
        },
    )
}

@Composable
@Suppress("LongParameterList")
private fun JunoOnboardingContent(
    pagesToDisplay: List<OnboardingPageUiData>,
    pagerState: PagerState,
    onMakeFirefoxDefaultClick: () -> Unit,
    onMakeFirefoxDefaultSkipClick: () -> Unit,
    onPrivacyPolicyClick: (String) -> Unit,
    onSignInButtonClick: () -> Unit,
    onSignInSkipClick: () -> Unit,
    onNotificationPermissionButtonClick: () -> Unit,
    onNotificationPermissionSkipClick: () -> Unit,
) {
    val nestedScrollConnection = remember { DisableForwardSwipeNestedScrollConnection(pagerState) }

    Column(
        modifier = Modifier
            .background(FirefoxTheme.colors.layer1)
            .statusBarsPadding()
            .navigationBarsPadding(),
    ) {
        HorizontalPager(
            pageCount = pagesToDisplay.size,
            state = pagerState,
            key = { pagesToDisplay[it].type },
            modifier = Modifier
                .weight(1f)
                .nestedScroll(nestedScrollConnection),
        ) { pageIndex ->
            val pageUiState = pagesToDisplay[pageIndex]
            val onboardingPageState = mapToOnboardingPageState(
                onboardingPageUiData = pageUiState,
                onMakeFirefoxDefaultClick = onMakeFirefoxDefaultClick,
                onMakeFirefoxDefaultSkipClick = onMakeFirefoxDefaultSkipClick,
                onPrivacyPolicyClick = onPrivacyPolicyClick,
                onSignInButtonClick = onSignInButtonClick,
                onSignInSkipClick = onSignInSkipClick,
                onNotificationPermissionButtonClick = onNotificationPermissionButtonClick,
                onNotificationPermissionSkipClick = onNotificationPermissionSkipClick,
            )
            OnboardingPage(
                pageState = onboardingPageState,
                imageResContentScale = pageUiState.imageResContentScale,
            )
        }

        PagerIndicator(
            pagerState = pagerState,
            pageCount = pagesToDisplay.size,
            activeColor = FirefoxTheme.colors.actionPrimary,
            inactiveColor = FirefoxTheme.colors.actionSecondary,
            leaveTrail = true,
            modifier = Modifier
                .align(Alignment.CenterHorizontally)
                .padding(bottom = 16.dp),
        )
    }
}

private class DisableForwardSwipeNestedScrollConnection(
    private val pagerState: PagerState,
) : NestedScrollConnection {

    override fun onPreScroll(available: Offset, source: NestedScrollSource): Offset =
        if (available.x > 0) {
            // Allow going back on swipe
            Offset.Zero
        } else {
            // For forward swipe, only allow if the visible item offset is less than 0,
            // this would be a result of a slow back fling, and we should allow snapper to
            // snap to the appropriate item.
            // Else consume the whole offset and disable going forward.
            if (pagerState.currentPageOffsetFraction < 0) {
                Offset.Zero
            } else {
                Offset(available.x, 0f)
            }
        }
}

@LightDarkPreview
@Composable
private fun JunoOnboardingScreenPreview() {
    FirefoxTheme {
        JunoOnboardingContent(
            pagesToDisplay = defaultPreviewPages(),
            pagerState = PagerState(0),
            onMakeFirefoxDefaultClick = {},
            onMakeFirefoxDefaultSkipClick = {},
            onPrivacyPolicyClick = {},
            onSignInButtonClick = {},
            onSignInSkipClick = {},
            onNotificationPermissionButtonClick = {},
            onNotificationPermissionSkipClick = {},
        )
    }
}

@Composable
private fun defaultPreviewPages() = listOf(
    OnboardingPageUiData(
        type = OnboardingPageUiData.Type.DEFAULT_BROWSER,
        imageRes = R.drawable.ic_onboarding_welcome,
        imageResContentScale = ContentScale.Fit,
        title = stringResource(R.string.juno_onboarding_default_browser_title_nimbus),
        description = stringResource(R.string.juno_onboarding_default_browser_description_nimbus),
        linkText = stringResource(R.string.juno_onboarding_default_browser_description_link_text),
        primaryButtonLabel = stringResource(R.string.juno_onboarding_default_browser_positive_button),
        secondaryButtonLabel = stringResource(R.string.juno_onboarding_default_browser_negative_button),
    ),
    OnboardingPageUiData(
        type = OnboardingPageUiData.Type.SYNC_SIGN_IN,
        imageRes = R.drawable.ic_onboarding_sync,
        imageResContentScale = ContentScale.Fit,
        title = stringResource(R.string.juno_onboarding_sign_in_title),
        description = stringResource(R.string.juno_onboarding_sign_in_description),
        primaryButtonLabel = stringResource(R.string.juno_onboarding_sign_in_positive_button),
        secondaryButtonLabel = stringResource(R.string.juno_onboarding_sign_in_negative_button),
    ),
    OnboardingPageUiData(
        type = OnboardingPageUiData.Type.NOTIFICATION_PERMISSION,
        imageRes = R.drawable.ic_notification_permission,
        imageResContentScale = ContentScale.Fit,
        title = stringResource(R.string.juno_onboarding_enable_notifications_title_nimbus),
        description = stringResource(R.string.juno_onboarding_enable_notifications_description_nimbus),
        primaryButtonLabel = stringResource(R.string.juno_onboarding_enable_notifications_positive_button),
        secondaryButtonLabel = stringResource(R.string.juno_onboarding_enable_notifications_negative_button),
    ),
)
