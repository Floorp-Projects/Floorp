/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.wallpaper

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.getValue
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.platform.ComposeView
import androidx.compose.ui.platform.ViewCompositionStrategy
import androidx.fragment.app.Fragment
import org.mozilla.fenix.ext.requireComponents
import org.mozilla.fenix.theme.FirefoxTheme
import org.mozilla.fenix.wallpapers.Wallpaper

class WallpaperSettingsFragment : Fragment() {
    private val wallpaperManager by lazy {
        requireComponents.wallpaperManager
    }

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View? {
        return ComposeView(requireContext()).apply {
            setViewCompositionStrategy(ViewCompositionStrategy.DisposeOnViewTreeLifecycleDestroyed)
            setContent {
                FirefoxTheme {
                    var currentWallpaper by remember { mutableStateOf(wallpaperManager.currentWallpaper) }
                    WallpaperSettings(
                        wallpapers = Wallpaper.values().toList(),
                        selectedWallpaper = currentWallpaper,
                        onSelectWallpaper = { selectedWallpaper: Wallpaper ->
                            currentWallpaper = selectedWallpaper
                            wallpaperManager.currentWallpaper = selectedWallpaper
                        }
                    )
                }
            }
        }
    }
}
