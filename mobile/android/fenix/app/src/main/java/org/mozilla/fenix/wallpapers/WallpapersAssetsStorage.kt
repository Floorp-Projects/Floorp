/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.wallpapers

import android.content.Context
import android.content.res.AssetManager
import mozilla.components.support.base.log.logger.Logger
import org.json.JSONArray
import org.json.JSONException
import org.json.JSONObject
import java.lang.Exception

class WallpapersAssetsStorage(private val context: Context) : WallpaperStorage {
    val logger = Logger("WallpapersAssetsStorage")
    private val wallpapersDirectory = "wallpapers"

    @Suppress("TooGenericExceptionCaught")
    override fun loadAll(): List<Wallpaper> {
        val assetsManager = context.assets
        return try {
            assetsManager.readArray("$wallpapersDirectory/wallpapers.json").toWallpapers()
        } catch (e: Exception) {
            logger.error("Unable to load wallpaper", e)
            emptyList()
        }
    }

    private fun JSONArray.toWallpapers(): List<Wallpaper> {
        return (0 until this.length()).mapNotNull { index ->
            this.getJSONObject(index).toWallpaper()
        }
    }

    private fun JSONObject.toWallpaper(): Wallpaper? {
        return try {
            Wallpaper(
                name = getString("name"),
                portraitPath = getString("portrait"),
                landscapePath = getString("landscape"),
                isDark = getBoolean("isDark"),
                themeCollection = Result.runCatching {
                    when (getString("themeCollection")) {
                        "firefox" -> WallpaperThemeCollection.Firefox
                        else -> WallpaperThemeCollection.None
                    }
                }.getOrDefault(WallpaperThemeCollection.None)
            )
        } catch (e: JSONException) {
            logger.error("unable to parse json for wallpaper $this", e)
            null
        }
    }

    private fun AssetManager.readArray(fileName: String) = JSONArray(
        open(fileName).bufferedReader().use {
            it.readText()
        }
    )
}
