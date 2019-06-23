/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings

import android.content.pm.PackageManager
import android.os.Build
import android.os.Build.VERSION.SDK_INT
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.appcompat.app.AppCompatActivity
import androidx.core.content.pm.PackageInfoCompat
import androidx.fragment.app.Fragment
import kotlinx.android.synthetic.main.fragment_about.*
import org.mozilla.fenix.BuildConfig
import org.mozilla.fenix.R
import org.mozilla.geckoview.BuildConfig as GeckoViewBuildConfig

class AboutFragment : Fragment() {
    override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?): View? {
        return inflater.inflate(R.layout.fragment_about, container, false)
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        val appName = requireContext().resources.getString(R.string.app_name)
        (activity as AppCompatActivity).title = getString(R.string.preferences_about, appName)
        val maybeGecko = if (SDK_INT < Build.VERSION_CODES.N) {
            "GV: "
        } else {
            " \uD83E\uDD8E "
        }

        val aboutText = try {
            val packageInfo = requireContext().packageManager.getPackageInfo(requireContext().packageName, 0)
            val geckoVersion = PackageInfoCompat.getLongVersionCode(packageInfo).toString() + maybeGecko +
                    GeckoViewBuildConfig.MOZ_APP_VERSION + "-" + GeckoViewBuildConfig.MOZ_APP_BUILDID
            val componentsVersion = mozilla.components.Build.version

            String.format(
                "%s (Build #%s)\nAC: %s",
                packageInfo.versionName,
                geckoVersion,
                componentsVersion
            )
        } catch (e: PackageManager.NameNotFoundException) {
            ""
        }

        val buildDate = BuildConfig.BUILD_DATE
        val content = resources.getString(R.string.about_content, appName)

        about_text.text = aboutText
        about_content.text = content
        build_date.text = buildDate
    }
}
