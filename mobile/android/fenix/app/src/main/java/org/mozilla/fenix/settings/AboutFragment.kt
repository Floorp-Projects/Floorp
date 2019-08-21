/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings

import android.content.Intent
import android.content.pm.PackageManager
import android.os.Build
import android.os.Build.VERSION.SDK_INT
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.annotation.RequiresApi
import androidx.core.content.pm.PackageInfoCompat
import androidx.fragment.app.Fragment
import com.google.android.gms.oss.licenses.OssLicensesMenuActivity
import kotlinx.android.synthetic.main.fragment_about.*
import org.mozilla.fenix.BuildConfig
import org.mozilla.fenix.R
import org.mozilla.geckoview.BuildConfig as GeckoViewBuildConfig

/**
 * Displays the logo and information about the app, including library versions.
 */
class AboutFragment : Fragment() {

    override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?): View? {
        return inflater.inflate(R.layout.fragment_about, container, false)
    }

    /**
     * Sets the activity title, displays library version strings, and sets up the [view_licenses_button].
     */
    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        val appName = getString(R.string.app_name)
        activity?.title = getString(R.string.preferences_about, appName)

        val aboutText = try {
            val packageInfo = requireContext().packageManager.getPackageInfo(requireContext().packageName, 0)
            val versionCode = PackageInfoCompat.getLongVersionCode(packageInfo).toString()
            val componentsVersion = mozilla.components.Build.version + ", " + mozilla.components.Build.gitHash
            val maybeGecko = if (SDK_INT >= Build.VERSION_CODES.N) GECKO_EMOJI else "GV"
            val geckoVersion = GeckoViewBuildConfig.MOZ_APP_VERSION + "-" + GeckoViewBuildConfig.MOZ_APP_BUILDID

            String.format(
                "%s (Build #%s)\n%s: %s\n%s: %s",
                packageInfo.versionName,
                versionCode,
                COMPONENTS_EMOJI,
                componentsVersion,
                maybeGecko,
                geckoVersion
            )
        } catch (e: PackageManager.NameNotFoundException) {
            ""
        }

        val content = getString(R.string.about_content, appName)
        val buildDate = BuildConfig.BUILD_DATE

        about_text.text = aboutText
        about_content.text = content
        build_date.text = buildDate

        view_licenses_button.setOnClickListener {
            startActivity(Intent(context, OssLicensesMenuActivity::class.java))
            OssLicensesMenuActivity.setActivityTitle(getString(R.string.open_source_licenses_title, appName))
        }
    }

    companion object {
        private const val COMPONENTS_EMOJI = "\uD83D\uDCE6"
        @RequiresApi(Build.VERSION_CODES.N)
        private const val GECKO_EMOJI = "\uD83E\uDD8E"
    }
}
