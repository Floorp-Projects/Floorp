/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

package org.mozilla.gecko.util;

import android.content.Context;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.text.TextUtils;

public class ContextUtils {
    private static final String INSTALLER_GOOGLE_PLAY = "com.android.vending";

    private ContextUtils() {}

    /**
     * @return {@link android.content.pm.PackageInfo#firstInstallTime} for the context's package.
     */
    public static PackageInfo getCurrentPackageInfo(final Context context) {
        try {
            return context.getPackageManager().getPackageInfo(context.getPackageName(), 0);
        } catch (PackageManager.NameNotFoundException e) {
            throw new AssertionError("Should not happen: Can't get package info of own package");
        }
    }

    public static boolean isPackageInstalled(final Context context, String packageName) {
        try {
            PackageManager pm = context.getPackageManager();
            pm.getPackageInfo(packageName, 0);
            return true;
        } catch (PackageManager.NameNotFoundException e) {
            return false;
        }
    }

    public static boolean isInstalledFromGooglePlay(final Context context) {
        final String installerPackageName = context.getPackageManager().getInstallerPackageName(context.getPackageName());

        if (TextUtils.isEmpty(installerPackageName)) {
            return false;
        }

        return INSTALLER_GOOGLE_PLAY.equals(installerPackageName);
    }
}
