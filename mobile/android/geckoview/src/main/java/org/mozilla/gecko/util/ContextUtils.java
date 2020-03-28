/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

package org.mozilla.gecko.util;

import android.content.Context;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.os.Build;
import android.provider.Settings;
import android.support.annotation.NonNull;
import android.text.TextUtils;

public class ContextUtils {
    private static final String INSTALLER_GOOGLE_PLAY = "com.android.vending";

    private ContextUtils() {}

    public static PackageInfo getCurrentPackageInfo(final Context context, final int flags) {
        try {
            return context.getPackageManager().getPackageInfo(context.getPackageName(), flags);
        } catch (PackageManager.NameNotFoundException e) {
            throw new AssertionError("Should not happen: Can't get package info of own package");
        }
    }

    public static PackageInfo getCurrentPackageInfo(final Context context) {
        return getCurrentPackageInfo(context, 0);
    }

    public static boolean isApplicationDebuggable(final @NonNull Context context) {
        final ApplicationInfo applicationInfo = context.getApplicationInfo();
        return (applicationInfo.flags & ApplicationInfo.FLAG_DEBUGGABLE) != 0;
    }

    public static boolean isApplicationCurrentDebugApp(final @NonNull Context context) {
        final ApplicationInfo applicationInfo = context.getApplicationInfo();

        final String currentDebugApp;
        if (Build.VERSION.SDK_INT >= 17) {
            currentDebugApp = Settings.Global.getString(context.getContentResolver(),
                    Settings.Global.DEBUG_APP);
        } else {
            currentDebugApp = Settings.System.getString(context.getContentResolver(),
                    Settings.System.DEBUG_APP);
        }
        return applicationInfo.packageName.equals(currentDebugApp);
    }
}
