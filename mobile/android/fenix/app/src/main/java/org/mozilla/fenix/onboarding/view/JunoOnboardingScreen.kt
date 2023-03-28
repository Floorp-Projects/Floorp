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

private val OnboardingPageTypeList = listOf(
    JunoOnboardingPageType.DEFAULT_BROWSER,
    JunoOnboardingPageType.NOTIFICATION_PERMISSION,
    JunoOnboardingPageType.SYNC_SIGN_IN,
)

/**
 * A screen for displaying juno onboarding.
 *
 * @param onMakeFirefoxDefaultClick Invoked when the positive button on default browser page is
 * clicked.
 * @param onPrivacyPolicyClick Invoked when the privacy policy link text is clicked.
 * @param onSignInButtonClick Invoked when the positive button on the sign in page is clicked.
 * @param onNotificationPermissionButtonClick Invoked when the positive button on notification
 * page is clicked.
 * @param onFinish Invoked when the onboarding is completed.
 */
@Composable
fun JunoOnboardingScreen(
    onMakeFirefoxDefaultClick: () -> Unit,
    onPrivacyPolicyClick: (String) -> Unit,
    onSignInButtonClick: () -> Unit,
    onNotificationPermissionButtonClick: () -> Unit,
    onFinish: () -> Unit,
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
            onFinish()
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

    JunoOnboardingContent(
        pagerState = pagerState,
        scrollToNextPageOrDismiss = scrollToNextPageOrDismiss,
        onMakeFirefoxDefaultClick = onMakeFirefoxDefaultClick,
        onPrivacyPolicyClick = onPrivacyPolicyClick,
        onSignInButtonClick = onSignInButtonClick,
        onNotificationPermissionButtonClick = onNotificationPermissionButtonClick,
    )
}

@Composable
@Suppress("LongParameterList")
private fun JunoOnboardingContent(
    pagerState: PagerState,
    scrollToNextPageOrDismiss: () -> Unit,
    onMakeFirefoxDefaultClick: () -> Unit,
    onPrivacyPolicyClick: (String) -> Unit,
    onSignInButtonClick: () -> Unit,
    onNotificationPermissionButtonClick: () -> Unit,
) {
    val nestedScrollConnection = remember { DisableForwardSwipeNestedScrollConnection(pagerState) }

    Column(
        modifier = Modifier
            .background(FirefoxTheme.colors.layer1)
            .statusBarsPadding()
            .navigationBarsPadding(),
    ) {
        HorizontalPager(
            count = OnboardingPageTypeList.size,
            state = pagerState,
            key = { OnboardingPageTypeList[it] },
            modifier = Modifier
                .weight(1f)
                .nestedScroll(nestedScrollConnection),
        ) { pageIndex ->
            val onboardingPageType = OnboardingPageTypeList[pageIndex]
            val pageState = mapToOnboardingPageState(
                onboardingPageType = onboardingPageType,
                scrollToNextPageOrDismiss = scrollToNextPageOrDismiss,
                onMakeFirefoxDefaultClick = onMakeFirefoxDefaultClick,
                onPrivacyPolicyClick = onPrivacyPolicyClick,
                onSignInButtonClick = onSignInButtonClick,
                onNotificationPermissionButtonClick = onNotificationPermissionButtonClick,
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
                Offset(available.x, available.y)
            }
        }
    }
}

@LightDarkPreview
@Composable
private fun JunoOnboardingScreenPreview() {
    FirefoxTheme {
        JunoOnboardingContent(
            pagerState = PagerState(0),
            onMakeFirefoxDefaultClick = {},
            onPrivacyPolicyClick = {},
            onSignInButtonClick = {},
            onNotificationPermissionButtonClick = {},
            scrollToNextPageOrDismiss = {},
        )
    }
}
