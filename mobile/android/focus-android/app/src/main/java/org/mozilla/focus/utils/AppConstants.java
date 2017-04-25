/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.utils;

import org.mozilla.focus.BuildConfig;

public final class AppConstants {
    private static final String BUILD_TYPE_DEBUG = "debug";
    private static final String BUILD_TYPE_RELEASE = "release";

    private static final String PRODUCT_FLAVOR_KLAR = "klar";

    private AppConstants() {}

    public static boolean isDevBuild() {
        return BUILD_TYPE_DEBUG.equals(BuildConfig.BUILD_TYPE);
    }

    public static boolean isKlarBuild() {
        return PRODUCT_FLAVOR_KLAR.equals(BuildConfig.FLAVOR_product);
    }

    public static boolean isReleaseBuild() {
        return BUILD_TYPE_RELEASE.equals(BuildConfig.BUILD_TYPE);
    }

    public static boolean supportsDownloadingFiles() {
        // This feature is not on the roadmap for v1 - but let's keep it enabled in dev builds. (#14)
        return isDevBuild();
    }
}
