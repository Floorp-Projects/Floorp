/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.about

import android.content.Intent
import android.content.pm.PackageManager
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.core.content.pm.PackageInfoCompat
import androidx.fragment.app.Fragment
import androidx.recyclerview.widget.DividerItemDecoration
import com.google.android.gms.oss.licenses.OssLicensesMenuActivity
import kotlinx.android.synthetic.main.fragment_about.*
import kotlinx.coroutines.ExperimentalCoroutinesApi
import org.mozilla.fenix.BuildConfig
import org.mozilla.fenix.R
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.ext.requireComponents
import org.mozilla.fenix.lib.Do
import org.mozilla.fenix.settings.SupportUtils
import org.mozilla.fenix.settings.about.AboutItemType.LICENSING_INFO
import org.mozilla.fenix.settings.about.AboutItemType.PRIVACY_NOTICE
import org.mozilla.fenix.settings.about.AboutItemType.RIGHTS
import org.mozilla.fenix.settings.about.AboutItemType.SUPPORT
import org.mozilla.fenix.settings.about.AboutItemType.WHATS_NEW
import org.mozilla.fenix.whatsnew.WhatsNew
import org.mozilla.geckoview.BuildConfig as GeckoViewBuildConfig

/**
 * Displays the logo and information about the app, including library versions.
 */
class AboutFragment : Fragment(), AboutPageListener {
    private lateinit var appName: String
    private val aboutPageAdapter: AboutPageAdapter = AboutPageAdapter(this)

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View? {
        val rootView = inflater.inflate(R.layout.fragment_about, container, false)
        appName = getString(R.string.app_name)
        activity?.title = getString(R.string.preferences_about, appName)

        return rootView
    }

    @ExperimentalCoroutinesApi
    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        about_list.run {
            adapter = aboutPageAdapter
            addItemDecoration(
                DividerItemDecoration(
                    context,
                    DividerItemDecoration.VERTICAL
                )
            )
        }

        populateAboutHeader()
        aboutPageAdapter.updateData(populateAboutList())
    }

    private fun populateAboutHeader() {
        val aboutText = try {
            val packageInfo = requireContext().packageManager.getPackageInfo(requireContext().packageName, 0)
            val versionCode = PackageInfoCompat.getLongVersionCode(packageInfo).toString()
            val componentsVersion = mozilla.components.Build.version + ", " + mozilla.components.Build.gitHash
            val maybeGecko = getString(R.string.gecko_view_abbreviation)
            val geckoVersion = GeckoViewBuildConfig.MOZ_APP_VERSION + "-" + GeckoViewBuildConfig.MOZ_APP_BUILDID
            val appServicesAbbreviation = getString(R.string.app_services_abbreviation)
            val appServicesVersion = mozilla.components.Build.applicationServicesVersion

            String.format(
                "%s (Build #%s)\n%s\n%s: %s\n%s: %s",
                packageInfo.versionName,
                versionCode,
                componentsVersion,
                maybeGecko,
                geckoVersion,
                appServicesAbbreviation,
                appServicesVersion
            )
        } catch (e: PackageManager.NameNotFoundException) {
            ""
        }

        val content = getString(R.string.about_content, appName)
        val buildDate = BuildConfig.BUILD_DATE

        about_text.text = aboutText
        about_content.text = content
        build_date.text = buildDate
    }

    private fun populateAboutList(): List<AboutPageItem> {
        val context = requireContext()

        return listOf(
            AboutPageItem.Item(
                AboutItem.ExternalLink(
                    WHATS_NEW,
                    SupportUtils.getWhatsNewUrl(context)
                ), getString(R.string.about_whats_new, getString(R.string.app_name))
            ),
            AboutPageItem.Item(
                AboutItem.ExternalLink(
                    SUPPORT,
                    SupportUtils.getSumoURLForTopic(context, SupportUtils.SumoTopic.HELP)
                ), getString(R.string.about_support)
            ),
            AboutPageItem.Item(
                AboutItem.ExternalLink(
                    PRIVACY_NOTICE,
                    SupportUtils.getPrivacyNoticeUrl()
                ), getString(R.string.about_privacy_notice)
            ),
            AboutPageItem.Item(
                AboutItem.ExternalLink(
                    RIGHTS,
                    SupportUtils.getSumoURLForTopic(context, SupportUtils.SumoTopic.YOUR_RIGHTS)
                ), getString(R.string.about_know_your_rights)
            ),
            AboutPageItem.Item(
                AboutItem.ExternalLink(LICENSING_INFO, ABOUT_LICENSE_URL),
                getString(R.string.about_licensing_information)
            ),
            AboutPageItem.Item(
                AboutItem.Libraries,
                getString(R.string.about_other_open_source_libraries)
            )
        )
    }

    private fun openLinkInCustomTab(url: String) {
        context?.let { context ->
            val intent = SupportUtils.createCustomTabIntent(context, url)
            startActivity(intent)
        }
    }

    private fun openLibrariesPage() {
        startActivity(Intent(context, OssLicensesMenuActivity::class.java))
        OssLicensesMenuActivity.setActivityTitle(getString(R.string.open_source_licenses_title, appName))
    }

    override fun onAboutItemClicked(item: AboutItem) {
        Do exhaustive when (item) {
            is AboutItem.ExternalLink -> {
                when (item.type) {
                    WHATS_NEW -> {
                        WhatsNew.userViewedWhatsNew(requireContext())
                        requireComponents.analytics.metrics.track(Event.WhatsNewTapped)
                    }
                    SUPPORT -> {
                        requireComponents.analytics.metrics.track(Event.SupportTapped)
                    }
                    PRIVACY_NOTICE -> {
                        requireComponents.analytics.metrics.track(Event.PrivacyNoticeTapped)
                    }
                    RIGHTS -> {
                        requireComponents.analytics.metrics.track(Event.RightsTapped)
                    }
                    LICENSING_INFO -> {
                        requireComponents.analytics.metrics.track(Event.LicensingTapped)
                    }
                }

                openLinkInCustomTab(item.url)
            }
            is AboutItem.Libraries -> {
                requireComponents.analytics.metrics.track(Event.LibrariesThatWeUseTapped)
                openLibrariesPage()
            }
        }
    }

    companion object {
        private const val ABOUT_LICENSE_URL = "about:license"
    }
}
