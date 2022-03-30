/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.fragment.onboarding

import android.content.Context
import android.os.Bundle
import android.transition.TransitionInflater
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.Button
import androidx.compose.material.ButtonDefaults
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.ColorFilter
import androidx.compose.ui.platform.ComposeView
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import androidx.constraintlayout.compose.ConstraintLayout
import androidx.constraintlayout.compose.Dimension
import androidx.fragment.app.Fragment
import org.mozilla.focus.GleanMetrics.Onboarding
import org.mozilla.focus.R
import org.mozilla.focus.ext.requireComponents
import org.mozilla.focus.ui.theme.FocusTheme
import org.mozilla.focus.ui.theme.focusColors
import org.mozilla.focus.ui.theme.focusTypography

class OnboardingFragment : Fragment() {

    private lateinit var onboardingInteractor: OnboardingInteractor

    override fun onAttach(context: Context) {
        super.onAttach(context)

        val transition =
            TransitionInflater.from(context).inflateTransition(R.transition.firstrun_exit)
        exitTransition = transition
        Onboarding.pageDisplayed.record(Onboarding.PageDisplayedExtra(0))
    }

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View {
        onboardingInteractor = OnboardingInteractor(requireComponents.appStore)
        return ComposeView(requireContext()).apply {
            setContent {
                FocusTheme {
                    OnboardingContent(onboardingInteractor)
                }
            }
        }
    }

    @Composable
    @Suppress("LongMethod")
    fun OnboardingContent(onboardingInteractor: OnboardingInteractor) {
        Box {
            Column(
                modifier = Modifier
                    .fillMaxWidth()
                    .fillMaxHeight()
                    .verticalScroll(rememberScrollState())
                    .background(color = focusColors.background)
                    .padding(top = 72.dp, start = 24.dp, end = 24.dp, bottom = 52.dp),
                horizontalAlignment = Alignment.CenterHorizontally
            ) {
                Image(
                    painter = painterResource(R.drawable.onboarding_logo),
                    contentDescription = getString(R.string.app_name),
                    modifier = Modifier.size(60.dp, 60.dp)
                )
                Text(
                    text = stringResource(id = R.string.onboarding_title, stringResource(id = R.string.app_name)),
                    style = focusTypography.onboardingTitle,
                    modifier = Modifier.padding(top = 16.dp, bottom = 20.dp)
                )
                Text(
                    text = stringResource(id = R.string.onboarding_description),
                    style = focusTypography.onboardingDescription,
                    modifier = Modifier.padding(bottom = 20.dp)
                )
                Column(
                    verticalArrangement = Arrangement.spacedBy(20.dp),
                    modifier = Modifier.padding(bottom = 36.dp)
                ) {
                    KeyFeatureCard(
                        iconId = R.drawable.mozac_ic_private_browsing,
                        titleId = R.string.onboarding_incognito_title,
                        descriptionId = R.string.onboarding_incognito_description
                    )
                    KeyFeatureCard(
                        iconId = R.drawable.ic_history,
                        titleId = R.string.onboarding_history_title,
                        descriptionId = R.string.onboarding_history_description2
                    )
                    KeyFeatureCard(
                        iconId = R.drawable.mozac_ic_settings,
                        titleId = R.string.onboarding_protection_title,
                        descriptionId = R.string.onboarding_protection_description
                    )
                }
                Button(
                    onClick = {
                        Onboarding.finishButtonTapped.record(Onboarding.FinishButtonTappedExtra())
                        onboardingInteractor.finishOnboarding(
                            requireContext(),
                            requireComponents.store.state.selectedTabId
                        )
                    },
                    modifier = Modifier
                        .width(232.dp)
                        .height(36.dp),
                    colors = ButtonDefaults.buttonColors(backgroundColor = focusColors.onboardingButtonBackground)
                ) {
                    Text(
                        text = stringResource(id = R.string.onboarding_start_browsing),
                        modifier = Modifier
                            .align(Alignment.CenterVertically),
                        style = focusTypography.onboardingButton,
                    )
                }
            }
        }
    }

    @Composable
    fun KeyFeatureCard(iconId: Int, titleId: Int, descriptionId: Int) {
        ConstraintLayout(modifier = Modifier.fillMaxWidth()) {
            val (image, title, description) = createRefs()
            Image(
                painter = painterResource(iconId),
                colorFilter = ColorFilter.tint(color = focusColors.onboardingKeyFeatureImageTint),
                contentDescription = getString(R.string.app_name),
                modifier = Modifier.constrainAs(image) {
                    top.linkTo(parent.top)
                    start.linkTo(parent.start)
                }
            )
            Text(
                text = stringResource(id = titleId),
                modifier = Modifier
                    .constrainAs(title) {
                        top.linkTo(image.top)
                        start.linkTo(image.end, margin = 16.dp)
                        bottom.linkTo(image.bottom)
                    },
                style = focusTypography.onboardingSubtitle
            )
            Text(
                text = stringResource(id = descriptionId, getString(R.string.onboarding_short_app_name)),
                modifier = Modifier.constrainAs(description) {
                    top.linkTo(title.bottom)
                    start.linkTo(title.start)
                    end.linkTo(parent.end)
                    width = Dimension.fillToConstraints
                },
                style = focusTypography.onboardingDescription
            )
        }
    }

    companion object {
        const val FRAGMENT_TAG = "firstrun"
        const val ONBOARDING_PREF = "firstrun_shown"

        fun create(): OnboardingFragment {
            val arguments = Bundle()

            val fragment = OnboardingFragment()
            fragment.arguments = arguments

            return fragment
        }
    }
}
