/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.activity

import android.app.Activity
import android.content.Context
import android.content.Intent
import android.content.pm.ActivityInfo
import android.os.Bundle
import android.webkit.WebView
import androidx.core.net.toUri
import mozilla.components.service.glean.private.NoExtras
import mozilla.components.support.utils.Browsers
import mozilla.components.support.utils.ext.resolveActivityCompat
import org.mozilla.focus.GleanMetrics.OpenWith
import org.mozilla.focus.utils.AppConstants

/**
 * Helper activity that will open the Google Play store by following a redirect URL.
 */
class InstallFirefoxActivity : Activity() {

    private var webView: WebView? = null

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        webView = WebView(this)

        setContentView(webView)

        webView!!.loadUrl(REDIRECT_URL)
    }

    override fun onPause() {
        super.onPause()

        if (webView != null) {
            webView!!.onPause()
        }

        finish()
    }

    override fun onDestroy() {
        super.onDestroy()

        if (webView != null) {
            webView!!.destroy()
        }
    }

    companion object {
        private const val REDIRECT_URL = "https://app.adjust.com/gs1ao4"

        fun resolveAppStore(context: Context): ActivityInfo? {
            val resolveInfo = context.packageManager.resolveActivityCompat(createStoreIntent(), 0)

            if (resolveInfo?.activityInfo == null) {
                return null
            }

            return if (!resolveInfo.activityInfo.exported) {
                // We are not allowed to launch this activity.
                null
            } else {
                resolveInfo.activityInfo
            }
        }

        private fun createStoreIntent(): Intent {
            return Intent(
                Intent.ACTION_VIEW,
                ("market://details?id=" + Browsers.KnownBrowser.FIREFOX.packageName).toUri(),
            )
        }

        fun open(context: Context) {
            if (AppConstants.isKlarBuild) {
                // Redirect to Google Play directly
                context.startActivity(createStoreIntent())
            } else {
                // Start this activity to load the redirect URL in a WebView.
                val intent = Intent(context, InstallFirefoxActivity::class.java)
                context.startActivity(intent)
            }

            OpenWith.installFirefox.record(NoExtras())
        }
    }
}
