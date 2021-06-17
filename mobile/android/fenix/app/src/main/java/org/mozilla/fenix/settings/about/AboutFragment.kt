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
import androidx.navigation.fragment.findNavController
import androidx.recyclerview.widget.DividerItemDecoration
import kotlinx.android.synthetic.main.fragment_about.*
import org.mozilla.fenix.BrowserDirection
import org.mozilla.fenix.BuildConfig
import org.mozilla.fenix.Config
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.crashes.CrashListActivity
import org.mozilla.fenix.ext.requireComponents
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.ext.showToolbar
import org.mozilla.fenix.settings.SupportUtils
import org.mozilla.fenix.settings.about.AboutItemType.LICENSING_INFO
import org.mozilla.fenix.settings.about.AboutItemType.PRIVACY_NOTICE
import org.mozilla.fenix.settings.about.AboutItemType.RIGHTS
import org.mozilla.fenix.settings.about.AboutItemType.SUPPORT
import org.mozilla.fenix.settings.about.AboutItemType.WHATS_NEW
import org.mozilla.fenix.utils.Do
import org.mozilla.fenix.whatsnew.WhatsNew
import org.mozilla.geckoview.BuildConfig as GeckoViewBuildConfig

/**
 * Displays the logo and information about the app, including library versions.
 */
class AboutFragment : Fragment(), AboutPageListener {

    private lateinit var headerAppName: String
    private lateinit var appName: String
    private var aboutPageAdapter: AboutPageAdapter? = AboutPageAdapter(this)

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View? {
        val rootView = inflater.inflate(R.layout.fragment_about, container, false)
        appName = getString(R.string.app_name)
        headerAppName =
            if (Config.channel.isRelease) getString(R.string.daylight_app_name) else appName
        showToolbar(getString(R.string.preferences_about, appName))

        return rootView
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        if (aboutPageAdapter == null) {
            aboutPageAdapter = AboutPageAdapter(this)
        }

        about_list.run {
            adapter = aboutPageAdapter
            addItemDecoration(
                DividerItemDecoration(
                    context,
                    DividerItemDecoration.VERTICAL
                )
            )
        }

        lifecycle.addObserver(
            SecretDebugMenuTrigger(
                logoView = wordmark,
                settings = view.context.settings()
            )
        )

        populateAboutHeader()
        aboutPageAdapter?.submitList(populateAboutList())
    }

    override fun onDestroyView() {
        super.onDestroyView()
        aboutPageAdapter = null
    }

    private fun populateAboutHeader() {
        val aboutText = try {
            val packageInfo =
                requireContext().packageManager.getPackageInfo(requireContext().packageName, 0)
            val versionCode = PackageInfoCompat.getLongVersionCode(packageInfo).toString()
            val componentsAbbreviation = getString(R.string.components_abbreviation)
            val componentsVersion =
                mozilla.components.Build.version + ", " + mozilla.components.Build.gitHash
            val maybeGecko = getString(R.string.gecko_view_abbreviation)
            val geckoVersion =
                GeckoViewBuildConfig.MOZ_APP_VERSION + "-" + GeckoViewBuildConfig.MOZ_APP_BUILDID
            val appServicesAbbreviation = getString(R.string.app_services_abbreviation)
            val appServicesVersion = mozilla.components.Build.applicationServicesVersion

            String.format(
                "%s (Build #%s)\n%s: %s\n%s: %s\n%s: %s",
                packageInfo.versionName,
                versionCode,
                componentsAbbreviation,
                componentsVersion,
                maybeGecko,
                geckoVersion,
                appServicesAbbreviation,
                appServicesVersion
            )
        } catch (e: PackageManager.NameNotFoundException) {
            ""
        }

        val content = getString(R.string.about_content, headerAppName)
        val buildDate = BuildConfig.BUILD_DATE

        about_text.text = aboutText
        about_content.text = content
        build_date.text = buildDate
    }

    private fun populateAboutList(): List<AboutPageItem> {
        val context = requireContext()

        return listOf(
            AboutPageItem(
                AboutItem.ExternalLink(
                    WHATS_NEW,
                    SupportUtils.getWhatsNewUrl(context)
                ), getString(R.string.about_whats_new, getString(R.string.app_name))
            ),
            AboutPageItem(
                AboutItem.ExternalLink(
                    SUPPORT,
                    SupportUtils.getSumoURLForTopic(context, SupportUtils.SumoTopic.HELP)
                ), getString(R.string.about_support)
            ),
            AboutPageItem(
                AboutItem.Crashes,
                getString(R.string.about_crashes)
            ),
            AboutPageItem(
                AboutItem.ExternalLink(
                    PRIVACY_NOTICE,
                    SupportUtils.getMozillaPageUrl(SupportUtils.MozillaPage.PRIVATE_NOTICE)
                ), getString(R.string.about_privacy_notice)
            ),
            AboutPageItem(
                AboutItem.ExternalLink(
                    RIGHTS,
                    SupportUtils.getSumoURLForTopic(context, SupportUtils.SumoTopic.YOUR_RIGHTS)
                ), getString(R.string.about_know_your_rights)
            ),
            AboutPageItem(
                AboutItem.ExternalLink(LICENSING_INFO, ABOUT_LICENSE_URL),
                getString(R.string.about_licensing_information)
            ),
            AboutPageItem(
                AboutItem.Libraries,
                getString(R.string.about_other_open_source_libraries)
            )
        )
    }

    private fun openLinkInNormalTab(url: String) {
        (activity as HomeActivity).openToBrowserAndLoad(
            searchTermOrURL = url,
            newTab = true,
            from = BrowserDirection.FromAbout
        )
    }

    private fun openLibrariesPage() {
        val navController = findNavController()
        navController.navigate(R.id.action_aboutFragment_to_aboutLibrariesFragment)
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
                    LICENSING_INFO, RIGHTS -> {} // no telemetry needed
                }

                openLinkInNormalTab(item.url)
            }
            is AboutItem.Libraries -> {
                openLibrariesPage()
            }
            is AboutItem.Crashes -> {
                startActivity(Intent(requireContext(), CrashListActivity::class.java))
            }
        }
    }

    companion object {
        private const val ABOUT_LICENSE_URL = "about:license"
    }
}
