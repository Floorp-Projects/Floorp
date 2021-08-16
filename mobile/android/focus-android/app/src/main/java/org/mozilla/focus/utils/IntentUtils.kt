/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.utils

import android.content.ActivityNotFoundException
import android.content.Context
import android.content.Intent
import android.net.Uri
import androidx.annotation.StringRes
import androidx.appcompat.app.AlertDialog
import org.mozilla.focus.R
import java.net.URISyntaxException

object IntentUtils {
    private const val MARKET_INTENT_URI_PACKAGE_PREFIX = "market://details?id="
    private const val EXTRA_BROWSER_FALLBACK_URL = "browser_fallback_url"

    /**
     * Find and open the appropriate app for a given Uri. If appropriate, let the user select between
     * multiple supported apps. Returns a boolean indicating whether the URL was handled. A fallback
     * URL will be opened in the supplied WebView if appropriate (in which case the URL was handled,
     * and true will also be returned). If not handled, we should  fall back to webviews error handling
     * (which ends up calling our error handling code as needed).
     *
     * Note: this method "leaks" the target Uri to Android before asking the user whether they
     * want to use an external app to open the uri. Ultimately the OS can spy on anything we're
     * doing in the app, so this isn't an actual "bug".
     */
    fun handleExternalUri(context: Context, uri: String): Boolean {
        // This code is largely based on Fennec's ExternalIntentDuringPrivateBrowsingPromptFragment.java
        val intent = try { Intent.parseUri(uri, 0) } catch (e: URISyntaxException) { return false }

        // Since we're a browser:
        intent.addCategory(Intent.CATEGORY_BROWSABLE)

        // This is where we "leak" the uri to the OS. If we're using the system webview, then the OS
        // already knows that we're opening this uri. Even if we're using GeckoView, the OS can still
        // see what domains we're visiting, so there's no loss of privacy here:
        val packageManager = context.packageManager
        val matchingActivities = packageManager.queryIntentActivities(intent, 0)

        when (matchingActivities.size) {
            0 -> handleUnsupportedLink(context, intent)
            1 -> {
                val info = matchingActivities[0]
                val externalAppTitle = info?.loadLabel(packageManager) ?: "(null)"

                showConfirmationDialog(
                    context,
                    intent,
                    context.getString(R.string.external_app_prompt_title),
                    R.string.external_app_prompt,
                    externalAppTitle
                )
            }
            else -> {
                val chooserTitle = context.getString(R.string.external_multiple_apps_matched_exit)
                val chooserIntent = Intent.createChooser(intent, chooserTitle)
                context.startActivity(chooserIntent)
            }
        }

        return true
    }

    private fun handleUnsupportedLink(context: Context, intent: Intent): Boolean {
        val fallbackUrl = intent.getStringExtra(EXTRA_BROWSER_FALLBACK_URL)
        if (fallbackUrl != null) {
            // webView.loadUrl(fallbackUrl)
            return true
        }

        if (intent.getPackage() != null) {
            // The url included the target package:
            val marketUri = MARKET_INTENT_URI_PACKAGE_PREFIX + intent.getPackage()!!
            val marketIntent = Intent(Intent.ACTION_VIEW, Uri.parse(marketUri))
            marketIntent.addCategory(Intent.CATEGORY_BROWSABLE)

            val packageManager = context.packageManager
            val info = packageManager.resolveActivity(marketIntent, 0)
            val marketTitle = info?.loadLabel(packageManager) ?: "(null)"

            showConfirmationDialog(
                context, marketIntent,
                context.getString(R.string.external_app_prompt_no_app_title),
                R.string.external_app_prompt_no_app, marketTitle
            )

            // Stop loading, we essentially have a result.
            return true
        }

        // If there's really no way to handle this, we just let the browser handle this URL
        // (which then shows the unsupported protocol page).
        return false
    }

    // We only need one param for both scenarios, hence we use just one "param" argument. If we ever
    // end up needing more or a variable number we can change this, but java varargs are a bit messy
    // so let's try to avoid that seeing as it's not needed right now.
    private fun showConfirmationDialog(
        context: Context,
        targetIntent: Intent,
        title: String,
        @StringRes messageResource: Int,
        param: CharSequence
    ) {
        val builder = AlertDialog.Builder(context, R.style.DialogStyle)
        val ourAppName = context.getString(R.string.app_name)

        builder.apply {
            setTitle(title)
            setMessage(context.resources.getString(messageResource, ourAppName, param))
            setPositiveButton(R.string.action_ok) { _, _ ->
                try {
                    context.startActivity(targetIntent)
                } catch (_: ActivityNotFoundException) {
                    return@setPositiveButton
                }
            }
            setNegativeButton(R.string.action_cancel) { dialog, _ -> dialog.dismiss() }
            show()
        }
    }

    fun activitiesFoundForIntent(context: Context, intent: Intent?): Boolean {
        return intent?.let {
            val packageManager = context.packageManager
            val resolveInfoList = packageManager.queryIntentActivities(intent, 0)
            resolveInfoList.size > 0
        } ?: false
    }

    fun createOpenFileIntent(uriFile: Uri?, mimeType: String?): Intent? {
        if (uriFile == null || mimeType == null) {
            return null
        }
        val openFileIntent = Intent(Intent.ACTION_VIEW)
        openFileIntent.setDataAndType(uriFile, mimeType)
        openFileIntent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
        return openFileIntent
    }
}
