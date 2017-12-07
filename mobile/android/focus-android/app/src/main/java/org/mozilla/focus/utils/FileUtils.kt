/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.utils

import android.content.Context
import java.io.File

// WebView directory and contents.
private const val WEBVIEW_DIRECTORY = "app_webview"
private const val LOCAL_STORAGE_DIR = "Local Storage"

// Cache directory contents.
private const val WEBVIEW_CACHE_DIR = "org.chromium.android_webview"

class FileUtils {
    companion object {
        @JvmStatic
        fun truncateCacheDirectory(context: Context) = deleteContent(context.cacheDir, doNotEraseWhitelist = setOf(
                // If the folder or its contents are deleted, WebView will stop using the disk cache entirely.
                WEBVIEW_CACHE_DIR
        ))

        @JvmStatic
        fun deleteWebViewDirectory(context: Context): Boolean {
            val webviewDirectory = File(context.applicationInfo.dataDir, WEBVIEW_DIRECTORY)
            return deleteContent(webviewDirectory, doNotEraseWhitelist = setOf(
                    // If the folder or its contents is deleted, WebStorage.deleteAllData does not clear Local Storage
                    // in memory.
                    LOCAL_STORAGE_DIR
            ))
        }

        private fun deleteContent(directory: File, doNotEraseWhitelist: Set<String> = emptySet()): Boolean {
            val filesToDelete = directory.listFiles()?.filter { !doNotEraseWhitelist.contains(it.name) } ?: return false
            return filesToDelete.all { it.deleteRecursively() }
        }
    }
}
