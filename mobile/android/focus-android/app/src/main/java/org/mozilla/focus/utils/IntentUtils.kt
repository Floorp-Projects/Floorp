/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.utils

import android.content.Context
import android.content.DialogInterface
import android.content.Intent
import android.content.pm.PackageManager
import android.content.pm.ResolveInfo
import android.net.Uri
import android.support.annotation.StringRes
import android.support.v7.app.AlertDialog

import org.mozilla.focus.R
import org.mozilla.focus.web.IWebView

import java.net.URISyntaxException

object IntentUtils {

    private val MARKET_INTENT_URI_PACKAGE_PREFIX = "market://details?id="
    private val EXTRA_BROWSER_FALLBACK_URL = "browser_fallback_url"

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
    fun handleExternalUri(context: Context, webView: IWebView, uri: String): Boolean {
        // This code is largely based on Fennec's ExternalIntentDuringPrivateBrowsingPromptFragment.java
        val intent: Intent
        try {
            intent = Intent.parseUri(uri, 0)
        } catch (e: URISyntaxException) {
            // Let the browser handle the url (i.e. let the browser show it's unsupported protocol /
            // invalid URL page).
            return false
        }

        // Since we're a browser:
        intent.addCategory(Intent.CATEGORY_BROWSABLE)

        val packageManager = context.packageManager

        // This is where we "leak" the uri to the OS. If we're using the system webview, then the OS
        // already knows that we're opening this uri. Even if we're using GeckoView, the OS can still
        // see what domains we're visiting, so there's no loss of privacy here:
        val matchingActivities = packageManager.queryIntentActivities(intent, 0)

        if (matchingActivities.size == 0) {
            return handleUnsupportedLink(context, webView, intent)
        } else if (matchingActivities.size == 1) {
            val info: ResolveInfo

            if (matchingActivities.size == 1) {
                info = matchingActivities[0]
            } else {
                // Ordering isn't guaranteed if there is more than one available activity - hence
                // we fetch the default (this code isn't currently run because we handle the > 1
                // case separately, but would be needed if we ever decide to prefer the default
                // app for the > 1 case.
                info = packageManager.resolveActivity(intent, 0)
            }
            val externalAppTitle = info.loadLabel(packageManager)

            showConfirmationDialog(context, intent, context.getString(R.string.external_app_prompt_title), R.string.external_app_prompt, externalAppTitle)
            return true
        } else { // matchingActivities.size() > 1
            // By explicitly showing the chooser, we can avoid having a (default) app from opening
            // the link immediately. This isn't perfect - we'd prefer to highlight the default app,
            // but it's not clear if there's any way of doing that. An alternative
            // would be to reuse the same dialog as for the single-activity case, and offer
            // a "open in other app this time" button if we have more than one matchingActivity.
            val chooserTitle = context.getString(R.string.external_multiple_apps_matched_exit)
            val chooserIntent = Intent.createChooser(intent, chooserTitle)
            context.startActivity(chooserIntent)

            return true
        }
    }

    private fun handleUnsupportedLink(context: Context, webView: IWebView, intent: Intent): Boolean {
        val fallbackUrl = intent.getStringExtra(EXTRA_BROWSER_FALLBACK_URL)
        if (fallbackUrl != null) {
            webView.loadUrl(fallbackUrl)
            return true
        }

        if (intent.getPackage() != null) {
            // The url included the target package:
            val marketUri = MARKET_INTENT_URI_PACKAGE_PREFIX + intent.getPackage()!!
            val marketIntent = Intent(Intent.ACTION_VIEW, Uri.parse(marketUri))
            marketIntent.addCategory(Intent.CATEGORY_BROWSABLE)

            val packageManager = context.packageManager
            val info = packageManager.resolveActivity(marketIntent, 0)
            val marketTitle = info.loadLabel(packageManager)
            showConfirmationDialog(context, marketIntent,
                    context.getString(R.string.external_app_prompt_no_app_title),
                    R.string.external_app_prompt_no_app, marketTitle)

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
    private fun showConfirmationDialog(context: Context, targetIntent: Intent, title: String, @StringRes messageResource: Int, param: CharSequence) {
        val builder = AlertDialog.Builder(context, R.style.DialogStyle)

        val ourAppName = context.getString(R.string.app_name)

        builder.setTitle(title)

        builder.setMessage(context.resources.getString(messageResource, ourAppName, param))

        builder.setPositiveButton(R.string.action_ok) { _, _ -> context.startActivity(targetIntent) }

        builder.setNegativeButton(R.string.action_cancel) { dialog, _-> dialog.dismiss() }

        // TODO: remove this, or enable it - depending on how we decide to handle the multiple-app/>1
        // case in future.
        //            if (matchingActivities.size() > 1) {
        //                builder.setNeutralButton(R.string.external_app_prompt_other, new DialogInterface.OnClickListener() {
        //                    @Override
        //                    public void onClick(DialogInterface dialog, int which) {
        //                        final String chooserTitle = activity.getResources().getString(R.string.external_multiple_apps_matched_exit);
        //                        final Intent chooserIntent = Intent.createChooser(intent, chooserTitle);
        //                        activity.startActivity(chooserIntent);
        //                    }
        //                });
        //            }

        builder.show()
    }

    fun activitiesFoundForIntent(context: Context, intent: Intent?): Boolean {
        if (intent == null) {
            return false
        }
        val packageManager = context.packageManager
        val resolveInfoList = packageManager.queryIntentActivities(intent, 0)
        return resolveInfoList.size > 0
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
