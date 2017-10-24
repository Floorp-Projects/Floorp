/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.utils

import android.content.Context

import java.io.File

private const val WEBVIEW_DIRECTORY = "app_webview"

class FileUtils {
    companion object {
        @JvmStatic
        fun truncateCacheDirectory(context: Context): Boolean {
            val cacheDirectory = context.cacheDir
            return cacheDirectory.exists() && deleteContent(cacheDirectory)
        }

        @JvmStatic
        fun deleteWebViewDirectory(context: Context): Boolean {
            val webviewDirectory = File(context.applicationInfo.dataDir, WEBVIEW_DIRECTORY)
            return webviewDirectory.deleteRecursively()
        }

        private fun deleteContent(directory: File) = directory.listFiles()?.all { it.deleteRecursively() } ?: false
    }
}
