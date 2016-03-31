/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

package org.mozilla.gecko.util;

import android.content.Context;
import android.content.pm.PackageManager;

public class ContextUtils {
    private ContextUtils() {}

    /**
     * @return {@link android.content.pm.PackageInfo#firstInstallTime} for the context's package.
     * @throws PackageManager.NameNotFoundException Unexpected - we get the package name from the context so
     *         it's expected to be found.
     */
    public static long getPackageInstallTime(final Context context) {
        try {
            return context.getPackageManager().getPackageInfo(context.getPackageName(), 0).firstInstallTime;
        } catch (final PackageManager.NameNotFoundException e) {
            throw new AssertionError("Should not happen: could not get package info for own package");
        }
    }
}
