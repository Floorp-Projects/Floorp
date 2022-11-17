/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.wallpapers

import android.graphics.Bitmap
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.ExperimentalMaterialApi
import androidx.compose.material.Icon
import androidx.compose.material.Surface
import androidx.compose.material.Text
import androidx.compose.material.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import org.mozilla.fenix.R
import org.mozilla.fenix.settings.wallpaper.WallpaperThumbnails
import org.mozilla.fenix.theme.FirefoxTheme

/**
 * A view that shows content of a WallpaperOnboarding dialog.
 *
 * @param wallpapers Wallpapers to add to grid.
 * @param currentWallpaper The currently selected wallpaper.
 * @param loadWallpaperResource Callback to handle loading a wallpaper bitmap. Only optional in the default case.
 * @param onCloseClicked Callback for when the close button is clicked.
 * @param onExploreMoreButtonClicked Callback for when the bottom text button is clicked.
 * @param onSelectWallpaper Callback for when a new wallpaper is selected.
 */

@Suppress("LongParameterList")
@ExperimentalMaterialApi
@Composable
fun WallpaperOnboarding(
    wallpapers: List<Wallpaper>,
    currentWallpaper: Wallpaper,
    loadWallpaperResource: suspend (Wallpaper) -> Bitmap?,
    onCloseClicked: () -> Unit,
    onExploreMoreButtonClicked: () -> Unit,
    onSelectWallpaper: (Wallpaper) -> Unit,
) {
    Surface(
        color = FirefoxTheme.colors.layer2,
        shape = RoundedCornerShape(topStart = 8.dp, topEnd = 8.dp),
    ) {
        Column(
            modifier = Modifier.padding(horizontal = 32.dp, vertical = 16.dp),
            horizontalAlignment = Alignment.CenterHorizontally,
        ) {
            Icon(
                painter = painterResource(id = R.drawable.mozac_ic_close),
                contentDescription = stringResource(id = R.string.close_tab),
                tint = FirefoxTheme.colors.iconPrimary,
                modifier = Modifier
                    .clickable { onCloseClicked() }
                    .size(24.dp)
                    .align(Alignment.End),
            )

            Spacer(modifier = Modifier.height(8.dp))

            Text(
                text = stringResource(R.string.wallpapers_onboarding_dialog_title_text),
                color = FirefoxTheme.colors.textPrimary,
                overflow = TextOverflow.Ellipsis,
                maxLines = 1,
                style = FirefoxTheme.typography.headline7,
            )

            Spacer(modifier = Modifier.height(4.dp))

            Text(
                text = stringResource(R.string.wallpapers_onboarding_dialog_body_text),
                color = FirefoxTheme.colors.textSecondary,
                overflow = TextOverflow.Ellipsis,
                maxLines = 1,
                style = FirefoxTheme.typography.caption,
            )

            Spacer(modifier = Modifier.height(16.dp))

            WallpaperThumbnails(
                wallpapers = wallpapers,
                defaultWallpaper = Wallpaper.Default,
                selectedWallpaper = currentWallpaper,
                loadWallpaperResource = { loadWallpaperResource(it) },
                onSelectWallpaper = { onSelectWallpaper(it) },
            )

            Spacer(modifier = Modifier.height(16.dp))

            TextButton(
                modifier = Modifier
                    .align(Alignment.CenterHorizontally)
                    .fillMaxWidth(),
                onClick = { onExploreMoreButtonClicked() },
            ) {
                Text(
                    text = stringResource(R.string.wallpapers_onboarding_dialog_explore_more_button_text),
                    color = FirefoxTheme.colors.textAccent,
                    overflow = TextOverflow.Ellipsis,
                    maxLines = 1,
                    style = FirefoxTheme.typography.button,
                )
            }
        }
    }
}

@Preview
@ExperimentalMaterialApi
@Composable
private fun WallpaperSnackbarPreview() {
    FirefoxTheme {
        WallpaperOnboarding(
            wallpapers = listOf(Wallpaper.Default),
            currentWallpaper = Wallpaper.Default,
            onCloseClicked = {},
            onExploreMoreButtonClicked = {},
            loadWallpaperResource = { null },
            onSelectWallpaper = {},
        )
    }
}
