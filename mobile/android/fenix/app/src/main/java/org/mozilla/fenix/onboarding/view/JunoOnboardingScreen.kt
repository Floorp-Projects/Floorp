/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.onboarding.view

import androidx.activity.compose.BackHandler
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.navigationBarsPadding
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.statusBarsPadding
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
import androidx.compose.ui.unit.dp
import com.google.accompanist.pager.HorizontalPager
import com.google.accompanist.pager.PagerState
import com.google.accompanist.pager.rememberPagerState
import kotlinx.coroutines.launch
import mozilla.components.lib.state.ext.observeAsComposableState
import org.mozilla.fenix.components.components
import org.mozilla.fenix.compose.PagerIndicator
import org.mozilla.fenix.compose.annotation.LightDarkPreview
import org.mozilla.fenix.theme.FirefoxTheme

/**
 * A screen for displaying juno onboarding.
 *
 * @param onboardingPageTypeList List of pages to be displayed in onboarding pager ui.
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
    onboardingPageTypeList: List<JunoOnboardingPageType>,
    onMakeFirefoxDefaultClick: () -> Unit,
    onSkipDefaultClick: () -> Unit,
    onPrivacyPolicyClick: (url: String) -> Unit,
    onSignInButtonClick: () -> Unit,
    onSkipSignInClick: () -> Unit,
    onNotificationPermissionButtonClick: () -> Unit,
    onSkipNotificationClick: () -> Unit,
    onFinish: (pageType: JunoOnboardingPageType) -> Unit,
    onImpression: (pageType: JunoOnboardingPageType) -> Unit,
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
        if (pagerState.currentPage == pagerState.pageCount - 1) {
            onFinish(onboardingPageTypeList[pagerState.currentPage])
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
            onImpression(onboardingPageTypeList[page])
        }
    }

    JunoOnboardingContent(
        onboardingPageTypeList = onboardingPageTypeList,
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
    onboardingPageTypeList: List<JunoOnboardingPageType>,
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
            count = onboardingPageTypeList.size,
            state = pagerState,
            key = { onboardingPageTypeList[it] },
            modifier = Modifier
                .weight(1f)
                .nestedScroll(nestedScrollConnection),
        ) { pageIndex ->
            val onboardingPageType = onboardingPageTypeList[pageIndex]
            val pageState = mapToOnboardingPageState(
                onboardingPageType = onboardingPageType,
                onMakeFirefoxDefaultClick = onMakeFirefoxDefaultClick,
                onMakeFirefoxDefaultSkipClick = onMakeFirefoxDefaultSkipClick,
                onPrivacyPolicyClick = onPrivacyPolicyClick,
                onSignInButtonClick = onSignInButtonClick,
                onSignInSkipClick = onSignInSkipClick,
                onNotificationPermissionButtonClick = onNotificationPermissionButtonClick,
                onNotificationPermissionSkipClick = onNotificationPermissionSkipClick,
            )
            OnboardingPage(pageState = pageState)
        }

        PagerIndicator(
            pagerState = pagerState,
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

    override fun onPreScroll(available: Offset, source: NestedScrollSource): Offset {
        return if (available.x > 0) {
            // Allow going back on swipe
            Offset.Zero
        } else {
            // For forward swipe, only allow if the visible item offset is less than 0,
            // this would be a result of a slow back fling, and we should allow snapper to
            // snap to the appropriate item.
            // Else consume the whole offset and disable going forward.
            if (pagerState.currentPageOffset < 0) {
                return Offset.Zero
            } else {
                Offset(available.x, 0f)
            }
        }
    }
}

@LightDarkPreview
@Composable
private fun JunoOnboardingScreenPreview() {
    FirefoxTheme {
        JunoOnboardingContent(
            onboardingPageTypeList = listOf(
                JunoOnboardingPageType.DEFAULT_BROWSER,
                JunoOnboardingPageType.SYNC_SIGN_IN,
                JunoOnboardingPageType.NOTIFICATION_PERMISSION,
            ),
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
