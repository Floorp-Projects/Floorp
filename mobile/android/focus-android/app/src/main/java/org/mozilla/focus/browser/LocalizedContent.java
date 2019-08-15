/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.browser;

import android.content.Context;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.res.Resources;
import android.view.View;

import androidx.annotation.NonNull;
import androidx.collection.ArrayMap;

import org.mozilla.focus.R;
import org.mozilla.focus.locale.Locales;
import org.mozilla.focus.utils.AppConstants;
import org.mozilla.focus.utils.HtmlLoader;
import org.mozilla.focus.utils.SupportUtils;
import org.mozilla.focus.web.IWebView;
import org.mozilla.geckoview.BuildConfig;

import java.util.Map;

public class LocalizedContent {
    // We can't use "about:" because webview silently swallows about: pages, hence we use
    // a custom scheme.
    public static final String URL_ABOUT = "focus:about";
    public static final String URL_RIGHTS = "focus:rights";

    public static boolean handleInternalContent(String url, IWebView webView, Context context) {
        if (URL_ABOUT.equals(url)) {
            loadAbout(webView, context);
            return true;
        } else if (URL_RIGHTS.equals(url)) {
            loadRights(webView, context);
            return true;
        }

        return false;
    }

    /**
     * Load the content for focus:about
     */
    private static void loadAbout(@NonNull final IWebView webView, Context context) {
        final Resources resources = Locales.getLocalizedResources(context);

        final Map<String, String> substitutionMap = new ArrayMap<>();
        final String appName = context.getResources().getString(R.string.app_name);
        final String learnMoreURL = SupportUtils.INSTANCE.getManifestoURL();

        String aboutVersion = "";
        try {
            final String engineIndicator = AppConstants.INSTANCE.isGeckoBuild() ?
                    " \uD83E\uDD8E " + BuildConfig.MOZ_APP_VERSION + "-" + BuildConfig.MOZ_APP_BUILDID : "";
            final PackageInfo packageInfo = context.getPackageManager().getPackageInfo(context.getPackageName(), 0);

            aboutVersion = String.format("%s (Build #%s)", packageInfo.versionName, packageInfo.versionCode + engineIndicator);
        } catch (PackageManager.NameNotFoundException e) {
            // Nothing to do if we can't find the package name.
        }
        substitutionMap.put("%about-version%", aboutVersion);

        final String aboutContent = resources.getString(R.string.about_content, appName, learnMoreURL);
        substitutionMap.put("%about-content%", aboutContent);

        final String wordmark = HtmlLoader.loadPngAsDataURI(context, R.drawable.wordmark);
        substitutionMap.put("%wordmark%", wordmark);

        putLayoutDirectionIntoMap(substitutionMap, context);

        final String data = HtmlLoader.loadResourceFile(context, R.raw.about, substitutionMap);

        webView.loadData("file:///android_res/raw/about.html", data, "text/html", "UTF-8", URL_ABOUT);
    }

    /**
     * Load the content for focus:rights
     */
    private static void loadRights(@NonNull final IWebView webView, Context context) {
        final Resources resources = Locales.getLocalizedResources(context);

        final Map<String, String> substitutionMap = new ArrayMap<>();

        final String appName = context.getResources().getString(R.string.app_name);
        final String mplUrl = "https://www.mozilla.org/en-US/MPL/";
        final String trademarkPolicyUrl = "https://www.mozilla.org/foundation/trademarks/policy/";
        final String gplUrl = "gpl.html";
        final String trackingProtectionUrl = "https://wiki.mozilla.org/Security/Tracking_protection#Lists";
        final String licensesUrl = "licenses.html";

        final String content1 = resources.getString(R.string.your_rights_content1, appName);
        substitutionMap.put("%your-rights-content1%", content1);

        final String content2 = resources.getString(R.string.your_rights_content2, appName, mplUrl);
        substitutionMap.put("%your-rights-content2%", content2);

        final String content3 = resources.getString(R.string.your_rights_content3, appName, trademarkPolicyUrl);
        substitutionMap.put("%your-rights-content3%", content3);

        final String content4 = resources.getString(R.string.your_rights_content4, appName, licensesUrl);
        substitutionMap.put("%your-rights-content4%", content4);

        final String content5 = resources.getString(R.string.your_rights_content5, appName, gplUrl, trackingProtectionUrl);
        substitutionMap.put("%your-rights-content5%", content5);

        putLayoutDirectionIntoMap(substitutionMap, context);

        final String data = HtmlLoader.loadResourceFile(context, R.raw.rights, substitutionMap);
        webView.loadData("file:///android_asset/rights.html", data, "text/html", "UTF-8", URL_RIGHTS);
    }

    private static void putLayoutDirectionIntoMap(Map<String, String> substitutionMap, Context context) {
        final int layoutDirection = context.getResources().getConfiguration().getLayoutDirection();
        final String direction;

        if (layoutDirection == View.LAYOUT_DIRECTION_LTR) {
            direction = "ltr";
        } else if (layoutDirection == View.LAYOUT_DIRECTION_RTL) {
            direction = "rtl";
        } else {
            direction = "auto";
        }

        substitutionMap.put("%dir%", direction);
    }
}
