/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.fragment.about

import android.os.Build
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
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.text.style.TextDirection
import androidx.compose.ui.unit.dp
import androidx.core.content.pm.PackageInfoCompat
import kotlinx.coroutines.Job
import mozilla.components.browser.state.state.SessionState
import mozilla.components.support.utils.ext.getPackageInfoCompat
import org.mozilla.focus.BuildConfig
import org.mozilla.focus.R
import org.mozilla.focus.databinding.FragmentAboutBinding
import org.mozilla.focus.ext.components
import org.mozilla.focus.ext.showToolbar
import org.mozilla.focus.settings.BaseSettingsLikeFragment
import org.mozilla.focus.state.AppAction
import org.mozilla.focus.ui.theme.FocusTheme
import org.mozilla.focus.ui.theme.focusColors
import org.mozilla.focus.ui.theme.focusTypography
import org.mozilla.focus.utils.SupportUtils.manifestoURL
import org.mozilla.geckoview.BuildConfig as GeckoViewBuildConfig

class AboutFragment : BaseSettingsLikeFragment() {

    private lateinit var secretSettingsUnlocker: SecretSettingsUnlocker

    private val openLearnMore = {
        val tabId = requireContext().components.tabsUseCases.addTab(
            url = manifestoURL,
            source = SessionState.Source.Internal.Menu,
            selectTab = true,
            private = true,
        )
        requireContext().components.appStore.dispatch(AppAction.OpenTab(tabId))
    }

    override fun onResume() {
        super.onResume()
        showToolbar(getString(R.string.menu_about))
        secretSettingsUnlocker.resetCounter()
    }

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?,
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

        secretSettingsUnlocker = SecretSettingsUnlocker(requireContext())

        binding.aboutPageContent.setContent {
            AboutPageContent(
                getAboutHeader(),
                content,
                learnMore,
                secretSettingsUnlocker,
                openLearnMore,
            )
        }
        return binding.root
    }

    private fun getAboutHeader(): String {
        val gecko = if (Build.VERSION.SDK_INT > Build.VERSION_CODES.M) " \uD83E\uDD8E " else " GV: "
        val engineIndicator = gecko + GeckoViewBuildConfig.MOZ_APP_VERSION + "-" + GeckoViewBuildConfig.MOZ_APP_BUILDID
        val servicesAbbreviation = getString(R.string.services_abbreviation)
        val servicesIndicator = mozilla.components.Build.applicationServicesVersion
        val packageInfo = requireContext().packageManager.getPackageInfoCompat(requireContext().packageName, 0)
        val versionCode = PackageInfoCompat.getLongVersionCode(packageInfo).toString()
        val vcsHash = if (BuildConfig.VCS_HASH.isNotBlank()) ", ${BuildConfig.VCS_HASH}" else ""

        @Suppress("ImplicitDefaultLocale") // We want LTR in all cases as the version is not translatable.
        return String.format(
            "%s (Build #%s)%s\n%s: %s",
            packageInfo.versionName,
            versionCode + engineIndicator,
            vcsHash,
            servicesAbbreviation,
            servicesIndicator,
        )
    }
}

@Composable
private fun AboutPageContent(
    aboutVersion: String,
    content: String,
    learnMore: String,
    secretSettingsUnlocker: SecretSettingsUnlocker,
    openLearnMore: () -> Job,
) {
    FocusTheme {
        Column(
            modifier = Modifier
                .padding(8.dp)
                .fillMaxSize()
                .verticalScroll(rememberScrollState()),
            verticalArrangement = Arrangement.Center,
            horizontalAlignment = Alignment.CenterHorizontally,

        ) {
            LogoIcon(secretSettingsUnlocker)
            VersionInfo(aboutVersion)
            AboutContent(content)
            LearnMoreLink(learnMore, openLearnMore)
        }
    }
}

@Composable
private fun LogoIcon(secretSettingsUnlocker: SecretSettingsUnlocker) {
    Image(
        painter = painterResource(R.drawable.wordmark2),
        contentDescription = null,
        modifier = Modifier
            .padding(4.dp)
            .clickable {
                secretSettingsUnlocker.increment()
            },
    )
}

@Composable
private fun VersionInfo(aboutVersion: String) {
    Text(
        text = aboutVersion,
        color = focusColors.aboutPageText,
        style = focusTypography.body1.copy(
            // Use LTR in all cases since the version is not translatable.
            textDirection = TextDirection.Ltr,
        ),
        modifier = Modifier
            .padding(10.dp),
    )
}

@Composable
private fun AboutContent(content: String) {
    Text(
        text = content,
        color = focusColors.aboutPageText,
        style = focusTypography.body1,
        modifier = Modifier
            .padding(10.dp),
    )
}

@Composable
fun ColumnScope.LearnMoreLink(
    learnMore: String,
    openLearnMore: () -> Job,
) {
    Text(
        text = learnMore,
        color = focusColors.aboutPageLink,
        style = focusTypography.links,
        modifier = Modifier
            .padding(10.dp)
            .fillMaxWidth()
            .align(Start)
            .clickable {
                openLearnMore()
            },
    )
}
