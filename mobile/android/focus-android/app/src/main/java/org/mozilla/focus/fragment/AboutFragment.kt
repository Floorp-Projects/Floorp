/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.fragment

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.compose.foundation.Image
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ColumnScope
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Alignment.Companion.Start
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.res.colorResource
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.style.TextDecoration
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import kotlinx.coroutines.Job
import mozilla.components.browser.state.state.SessionState
import org.mozilla.focus.R
import org.mozilla.focus.databinding.FragmentAboutBinding
import org.mozilla.focus.ext.components
import org.mozilla.focus.settings.BaseSettingsLikeFragment
import org.mozilla.focus.state.AppAction
import org.mozilla.focus.utils.SupportUtils.manifestoURL
import org.mozilla.geckoview.BuildConfig

class AboutFragment : BaseSettingsLikeFragment() {

    private val openLearnMore = {
        val tabId = requireContext().components.tabsUseCases.addTab(
            url = manifestoURL,
            source = SessionState.Source.Internal.Menu,
            selectTab = true,
            private = true
        )
        requireContext().components.appStore.dispatch(AppAction.OpenTab(tabId))
    }

    override fun onResume() {
        super.onResume()
        updateTitle("About")
    }

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View {
        val binding = FragmentAboutBinding.inflate(inflater, container, false)

        val appName = requireContext().resources.getString(R.string.app_name)
        val aboutContent =
            requireContext().getString(R.string.about_content, appName, "")

        val learnMore = aboutContent
            .substringAfter("<a href=>")
            .substringBefore("</a></p>")

        val content =
            aboutContent
                .replace("<li>", "\u2022 \u0009 ")
                .replace("</li>", "\n")
                .replace("<ul>", "\n \n")
                .replace("</ul>", "")
                .replace("<p>", "\n")
                .replace("</p>", "")
                .replaceAfter("<br/>", "")
                .replace("<br/>", "")

        val engineIndicator =
            " \uD83E\uDD8E " + BuildConfig.MOZ_APP_VERSION + "-" + BuildConfig.MOZ_APP_BUILDID
        val packageInfo =
            requireContext().packageManager.getPackageInfo(requireContext().packageName, 0)

        @Suppress("ImplicitDefaultLocale")
        val aboutVersion = String.format(
            "%s (Build #%s)",
            packageInfo.versionName,
            @Suppress("DEPRECATION")
            packageInfo.versionCode.toString() + engineIndicator
        )

        binding.aboutPageContent.setContent {
            AboutPageContent(aboutVersion, content, learnMore, openLearnMore)
        }
        return binding.root
    }
}

@Composable
private fun AboutPageContent(
    aboutVersion: String,
    content: String,
    learnMore: String,
    openLearnMore: () -> Job
) {
    Column(
        modifier = Modifier
            .padding(8.dp)
            .fillMaxSize()
            .verticalScroll(rememberScrollState()),
        verticalArrangement = Arrangement.Center,
        horizontalAlignment = Alignment.CenterHorizontally

    ) {
        LogoIcon()
        VersionInfo(aboutVersion)
        AboutContent(content)
        LearnMoreLink(learnMore, openLearnMore)
    }
}

@Composable
private fun LogoIcon() {
    Image(
        painter = painterResource(R.drawable.wordmark2),
        contentDescription = null,
        modifier = Modifier
            .padding(4.dp)
    )
}

@Composable
private fun VersionInfo(aboutVersion: String) {
    Text(
        text = aboutVersion,
        color = Color.White,
        fontSize = 16.sp,
        lineHeight = 24.sp,
        modifier = Modifier
            .padding(10.dp)
    )
}

@Composable
private fun AboutContent(content: String) {
    Text(
        text = content,
        color = Color.White,
        fontSize = 16.sp,
        lineHeight = 24.sp,
        modifier = Modifier
            .padding(10.dp)
    )
}

@Composable
fun ColumnScope.LearnMoreLink(
    learnMore: String,
    openLearnMore: () -> Job
) {
    Text(
        text = learnMore,
        color = colorResource(id = R.color.preference_learn_more_link),
        style = TextStyle(textDecoration = TextDecoration.Underline),
        fontSize = 16.sp,
        lineHeight = 24.sp,
        modifier = Modifier
            .padding(10.dp)
            .fillMaxWidth()
            .align(Start)
            .clickable {
                openLearnMore()
            }
    )
}
