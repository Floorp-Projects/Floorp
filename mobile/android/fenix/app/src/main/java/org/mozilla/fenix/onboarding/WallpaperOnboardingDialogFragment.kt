/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.onboarding

import android.annotation.SuppressLint
import android.content.DialogInterface
import android.content.pm.ActivityInfo
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.compose.material.ExperimentalMaterialApi
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.ui.platform.ComposeView
import androidx.compose.ui.platform.ViewCompositionStrategy
import androidx.navigation.fragment.findNavController
import com.google.android.material.bottomsheet.BottomSheetDialogFragment
import kotlinx.coroutines.launch
import mozilla.components.lib.state.ext.observeAsComposableState
import mozilla.telemetry.glean.private.NoExtras
import org.mozilla.fenix.GleanMetrics.Wallpapers
import org.mozilla.fenix.NavGraphDirections
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.requireComponents
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.theme.FirefoxTheme
import org.mozilla.fenix.wallpapers.Wallpaper
import org.mozilla.fenix.wallpapers.WallpaperOnboarding

/**
 * Dialog displaying the wallpapers onboarding.
 */
@OptIn(ExperimentalMaterialApi::class)
class WallpaperOnboardingDialogFragment : BottomSheetDialogFragment() {
    private val appStore by lazy {
        requireComponents.appStore
    }

    private val wallpaperUseCases by lazy {
        requireComponents.useCases.wallpaperUseCases
    }

    @SuppressLint("SourceLockedOrientationActivity")
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setStyle(STYLE_NO_TITLE, R.style.WallpaperOnboardingDialogStyle)
        activity?.requestedOrientation = ActivityInfo.SCREEN_ORIENTATION_PORTRAIT
    }

    override fun onDestroy() {
        super.onDestroy()
        activity?.requestedOrientation = ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED
    }

    override fun onDismiss(dialog: DialogInterface) {
        super.onDismiss(dialog)

        val currentWallpaper = requireContext().components.appStore.state.wallpaperState.currentWallpaper
        Wallpapers.onboardingClosed.record(
            Wallpapers.OnboardingClosedExtra(
                isSelected = currentWallpaper.name != Wallpaper.defaultName,
            ),
        )
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        requireContext().settings().showWallpaperOnboarding = false
        Wallpapers.onboardingOpened.record(NoExtras())
    }

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?,
    ): View = ComposeView(requireContext()).apply {
        setViewCompositionStrategy(ViewCompositionStrategy.DisposeOnViewTreeLifecycleDestroyed)

        setContent {
            FirefoxTheme {
                val wallpapers = appStore.observeAsComposableState { state ->
                    state.wallpaperState.availableWallpapers.take(THUMBNAILS_COUNT)
                }.value ?: listOf()
                val currentWallpaper = appStore.observeAsComposableState { state ->
                    state.wallpaperState.currentWallpaper
                }.value ?: Wallpaper.Default

                val coroutineScope = rememberCoroutineScope()

                WallpaperOnboarding(
                    wallpapers = wallpapers,
                    currentWallpaper = currentWallpaper,
                    onCloseClicked = { dismiss() },
                    onExploreMoreButtonClicked = {
                        val directions = NavGraphDirections.actionGlobalWallpaperSettingsFragment()
                        findNavController().navigate(directions)
                        Wallpapers.onboardingExploreMoreClick.record(NoExtras())
                    },
                    loadWallpaperResource = { wallpaperUseCases.loadThumbnail(it) },
                    onSelectWallpaper = {
                        coroutineScope.launch {
                            val result = wallpaperUseCases.selectWallpaper(it)
                            onWallpaperSelected(it, result)
                        }
                    },
                )
            }
        }
    }

    private fun onWallpaperSelected(
        wallpaper: Wallpaper,
        result: Wallpaper.ImageFileState,
    ) {
        if (result == Wallpaper.ImageFileState.Downloaded) {
            Wallpapers.wallpaperSelected.record(
                Wallpapers.WallpaperSelectedExtra(
                    name = wallpaper.name,
                    source = "onboarding",
                    themeCollection = wallpaper.collection.name,
                ),
            )
        }
    }

    companion object {
        const val THUMBNAILS_COUNT = 6
    }
}
