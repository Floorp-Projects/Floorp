/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.utils;

import android.content.Context;

import java.io.File;

public class FileUtils {
    private static final String WEBVIEW_DIRECTORY = "app_webview";

    public static boolean truncateCacheDirectory(final Context context) {
        final File cacheDirectory = context.getCacheDir();
        return cacheDirectory.exists() && deleteContent(cacheDirectory);
    }

    public static boolean deleteWebViewDirectory(final Context context) {
        final File webviewDirectory = new File(context.getApplicationInfo().dataDir, WEBVIEW_DIRECTORY);
        return webviewDirectory.exists() && deleteDirectory(webviewDirectory);
    }

    private static boolean deleteDirectory(File directory) {
        return deleteContent(directory) && directory.delete();
    }

    private static boolean deleteContent(File directory) {
        boolean success = true;

        for (final String name : directory.list()) {
            final File file = new File(directory, name);
            if (file.isDirectory()) {
                success &= deleteDirectory(file);
            } else {
                success &= file.delete();
            }
        }

        return success;
    }
}
