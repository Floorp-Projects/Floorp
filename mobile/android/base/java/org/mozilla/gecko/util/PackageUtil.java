/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.util;

import android.content.Context;
import android.content.Intent;
import android.content.pm.ResolveInfo;
import android.net.Uri;
import android.support.annotation.NonNull;
import android.text.TextUtils;

import java.util.List;

public class PackageUtil {

    private static final Uri TEST_URI = Uri.parse("https://www.mozilla.org");

    private PackageUtil() {
    }

    /**
     * Get information of user's default browser
     *
     * @param context
     * @return information of default browser, null if user hasn't set any default value.
     */
    public static ResolveInfo getDefaultBrowser(@NonNull Context context) {
        final Intent browserIntent = new Intent(Intent.ACTION_VIEW, TEST_URI);
        final int additionalFlags = 0; // no additional flags

        // this info might be activity picker
        final ResolveInfo resolveInfo = context.getPackageManager()
                .resolveActivity(browserIntent, additionalFlags);

        if (resolveInfo == null) {
            return null;
        }

        if (!resolveInfo.activityInfo.exported) {
            return null;
        }

        final List<ResolveInfo> browsers = resolveBrowsers(context);
        for (ResolveInfo it : browsers) {
            if (resolveInfo.activityInfo.packageName.equals(it.activityInfo.packageName)) {
                return resolveInfo;
            }
        }

        return null;
    }

    private static String getDefaultBrowserPackage(@NonNull final Context context) {
        final ResolveInfo resolveInfo = getDefaultBrowser(context);
        if (resolveInfo != null) {
            return resolveInfo.activityInfo.packageName;
        } else {
            return null;
        }
    }

    public static boolean isDefaultBrowser(Context context) {
        final String defaultBrowserPackageName = getDefaultBrowserPackage(context);
        return TextUtils.equals(defaultBrowserPackageName, context.getPackageName());
    }

    public static boolean isNoDefaultBrowserSet(Context context) {
        final String defaultBrowserPackageName = getDefaultBrowserPackage(context);
        return TextUtils.isEmpty(defaultBrowserPackageName);
    }

    public static void showInstalledBrowsers(Context context) {
        try {
            Intent intent = new Intent(Intent.ACTION_VIEW);
            intent.setData(Uri.parse("https://www.mozilla.org/firefox/mobile"));

            context.startActivity(intent);
        } catch (Exception e) {
            e.printStackTrace();
        }
    }

    /**
     * Information of activities which could handle a web-page-browsing intent.
     *
     * @param context
     * @return a List of activity's information. If default browser set, the list might only has one item.
     */
    private static List<ResolveInfo> resolveBrowsers(@NonNull Context context) {
        final Intent browserIntent = new Intent(Intent.ACTION_VIEW, TEST_URI);
        final int additionalFlags = 0; // no additional flags

        return context.getPackageManager().queryIntentActivities(browserIntent, additionalFlags);
    }
}
