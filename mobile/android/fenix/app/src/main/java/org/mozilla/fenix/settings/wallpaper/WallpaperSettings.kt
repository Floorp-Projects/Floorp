/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.wallpaper

import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.ExperimentalFoundationApi
import androidx.compose.foundation.Image
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.aspectRatio
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.GridCells
import androidx.compose.foundation.lazy.LazyVerticalGrid
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.Scaffold
import androidx.compose.material.Snackbar
import androidx.compose.material.SnackbarDuration
import androidx.compose.material.SnackbarHost
import androidx.compose.material.Surface
import androidx.compose.material.Text
import androidx.compose.material.TextButton
import androidx.compose.material.rememberScaffoldState
import androidx.compose.runtime.Composable
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.ui.Modifier
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.Font
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import kotlinx.coroutines.launch
import org.mozilla.fenix.R
import org.mozilla.fenix.theme.FirefoxTheme
import org.mozilla.fenix.wallpapers.Wallpaper
import java.util.Locale

/**
 * The screen for controlling settings around Wallpapers. When a new wallpaper is selected,
 * a snackbar will be displayed.
 *
 * @param wallpapers Wallpapers to add to grid.
 * @param selectedWallpaper The currently selected wallpaper.
 * @param onSelectWallpaper Callback for when a new wallpaper is selected.
 * @param onViewWallpaper Callback for when the view action is clicked from snackbar.
 */
@Composable
fun WallpaperSettings(
    wallpapers: List<Wallpaper>,
    selectedWallpaper: Wallpaper,
    onSelectWallpaper: (Wallpaper) -> Unit,
    onViewWallpaper: () -> Unit,
) {
    val coroutineScope = rememberCoroutineScope()
    val scaffoldState = rememberScaffoldState()

    Scaffold(
        backgroundColor = FirefoxTheme.colors.layer1,
        scaffoldState = scaffoldState,
        snackbarHost = { hostState ->
            SnackbarHost(hostState = hostState) {
                WallpaperSnackbar(onViewWallpaper)
            }
        }
    ) {
        Surface(color = FirefoxTheme.colors.layer2) {
            WallpaperThumbnails(
                wallpapers = wallpapers,
                selectedWallpaper = selectedWallpaper,
                onSelectWallpaper = { updatedWallpaper ->
                    coroutineScope.launch {
                        scaffoldState.snackbarHostState.showSnackbar(
                            message = "", // overwritten by WallpaperSnackbar
                            duration = SnackbarDuration.Short
                        )
                    }
                    onSelectWallpaper(updatedWallpaper)
                },
            )
        }
    }
}

@Composable
private fun WallpaperSnackbar(
    onViewWallpaper: () -> Unit,
) {
    Snackbar(
        modifier = Modifier
            .padding(8.dp)
            .heightIn(min = 48.dp),
        backgroundColor = FirefoxTheme.colors.actionPrimary,
        content = {
            Text(
                modifier = Modifier.padding(start = 16.dp, end = 16.dp, top = 8.dp, bottom = 8.dp),
                text = stringResource(R.string.wallpaper_updated_snackbar_message),
                textAlign = TextAlign.Start,
                color = FirefoxTheme.colors.textInverted,
                fontFamily = FontFamily(Font(R.font.metropolis_semibold)),
                fontSize = 18.sp,
                overflow = TextOverflow.Ellipsis,
                maxLines = 2
            )
        },
        action = {
            TextButton(
                modifier = Modifier.padding(start = 16.dp, end = 16.dp, top = 8.dp, bottom = 8.dp),
                onClick = onViewWallpaper,
            ) {
                Text(
                    text = stringResource(R.string.wallpaper_updated_snackbar_action).uppercase(
                        Locale.getDefault()
                    ),
                    color = FirefoxTheme.colors.textInverted,
                    fontFamily = FontFamily(Font(R.font.metropolis_medium)),
                    fontSize = 14.sp,
                )
            }
        },
    )
}

/**
 * A grid of selectable wallpaper thumbnails.
 *
 * @param wallpapers Wallpapers to add to grid.
 * @param selectedWallpaper The currently selected wallpaper.
 * @param numColumns The number of columns that will occupy the grid.
 * @param onSelectWallpaper Action to take when a new wallpaper is selected.
 */
@OptIn(ExperimentalFoundationApi::class)
@Composable
private fun WallpaperThumbnails(
    wallpapers: List<Wallpaper>,
    selectedWallpaper: Wallpaper,
    numColumns: Int = 3,
    onSelectWallpaper: (Wallpaper) -> Unit,
) {
    LazyVerticalGrid(
        cells = GridCells.Fixed(numColumns),
        modifier = Modifier.padding(vertical = 30.dp, horizontal = 20.dp)
    ) {
        items(wallpapers) { wallpaper ->
            WallpaperThumbnailItem(
                wallpaper = wallpaper,
                isSelected = selectedWallpaper == wallpaper,
                onSelect = onSelectWallpaper
            )
        }
    }
}

/**
 * A single wallpaper thumbnail.
 *
 * @param wallpaper The wallpaper to display.
 * @param isSelected Whether the wallpaper is currently selected.
 * @param aspectRatio The ratio of height to width of the thumbnail.
 * @param onSelect Action to take when this wallpaper is selected.
 */
@Composable
private fun WallpaperThumbnailItem(
    wallpaper: Wallpaper,
    isSelected: Boolean,
    aspectRatio: Float = 1.1f,
    onSelect: (Wallpaper) -> Unit
) {
    val thumbnailShape = RoundedCornerShape(8.dp)
    val border = if (isSelected) {
        Modifier.border(
            BorderStroke(width = 2.dp, color = FirefoxTheme.colors.borderAccent),
            thumbnailShape
        )
    } else {
        Modifier
    }

    Surface(
        elevation = 4.dp,
        shape = thumbnailShape,
        color = FirefoxTheme.colors.layer1,
        modifier = Modifier
            .fillMaxWidth()
            .aspectRatio(aspectRatio)
            .padding(4.dp)
            .then(border)
            .clickable { onSelect(wallpaper) }
    ) {
        if (wallpaper != Wallpaper.NONE) {
            val contentDescription = stringResource(
                R.string.wallpapers_item_name_content_description, wallpaper.name
            )
            Image(
                painterResource(id = wallpaper.drawable),
                contentScale = ContentScale.FillBounds,
                contentDescription = contentDescription,
                modifier = Modifier.fillMaxSize()
            )
        }
    }
}

@Preview
@Composable
private fun WallpaperThumbnailsPreview() {
    FirefoxTheme {
        WallpaperSettings(
            wallpapers = Wallpaper.values().toList(),
            selectedWallpaper = Wallpaper.NONE,
            onSelectWallpaper = {},
            onViewWallpaper = {},
        )
    }
}

@Preview
@Composable
private fun WallpaperSnackbarPreview() {
    FirefoxTheme {
        WallpaperSnackbar(onViewWallpaper = {})
    }
}
