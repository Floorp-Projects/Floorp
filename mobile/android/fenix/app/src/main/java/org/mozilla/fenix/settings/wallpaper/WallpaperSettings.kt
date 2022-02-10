/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.wallpaper

import android.graphics.Bitmap
import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.ExperimentalFoundationApi
import androidx.compose.foundation.Image
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
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
import androidx.compose.material.Switch
import androidx.compose.material.SwitchDefaults
import androidx.compose.material.Text
import androidx.compose.material.TextButton
import androidx.compose.material.rememberScaffoldState
import androidx.compose.runtime.Composable
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.platform.LocalContext
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
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.theme.FirefoxTheme
import org.mozilla.fenix.wallpapers.Wallpaper
import org.mozilla.fenix.wallpapers.WallpaperManager
import java.util.Locale

/**
 * The screen for controlling settings around Wallpapers. When a new wallpaper is selected,
 * a snackbar will be displayed.
 *
 * @param wallpapers Wallpapers to add to grid.
 * @param selectedWallpaper The currently selected wallpaper.
 * @param onSelectWallpaper Callback for when a new wallpaper is selected.
 * @param onViewWallpaper Callback for when the view action is clicked from snackbar.
 * @param tapLogoSwitchChecked Enabled state for switch controlling taps to change wallpaper.
 * @param onTapLogoSwitchCheckedChange Callback for when state of above switch is updated.
 */
@Composable
@Suppress("LongParameterList")
fun WallpaperSettings(
    wallpapers: List<Wallpaper>,
    defaultWallpaper: Wallpaper,
    loadWallpaperResource: (Wallpaper) -> Bitmap?,
    selectedWallpaper: Wallpaper,
    onSelectWallpaper: (Wallpaper) -> Unit,
    onViewWallpaper: () -> Unit,
    tapLogoSwitchChecked: Boolean,
    onTapLogoSwitchCheckedChange: (Boolean) -> Unit
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
        },
    ) {
        Column {
            WallpaperThumbnails(
                wallpapers = wallpapers,
                defaultWallpaper = defaultWallpaper,
                loadWallpaperResource = loadWallpaperResource,
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
            WallpaperLogoSwitch(tapLogoSwitchChecked, onCheckedChange = onTapLogoSwitchCheckedChange)
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
                color = FirefoxTheme.colors.textOnColor,
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
                    color = FirefoxTheme.colors.textOnColor,
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
@Suppress("LongParameterList")
private fun WallpaperThumbnails(
    wallpapers: List<Wallpaper>,
    defaultWallpaper: Wallpaper,
    loadWallpaperResource: (Wallpaper) -> Bitmap?,
    selectedWallpaper: Wallpaper,
    numColumns: Int = 3,
    onSelectWallpaper: (Wallpaper) -> Unit,
) {
    Surface(color = FirefoxTheme.colors.layer2) {
        LazyVerticalGrid(
            cells = GridCells.Fixed(numColumns),
            modifier = Modifier.padding(vertical = 30.dp, horizontal = 20.dp)
        ) {
            items(wallpapers) { wallpaper ->
                WallpaperThumbnailItem(
                    wallpaper = wallpaper,
                    defaultWallpaper = defaultWallpaper,
                    loadWallpaperResource = loadWallpaperResource,
                    isSelected = selectedWallpaper == wallpaper,
                    onSelect = onSelectWallpaper
                )
            }
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
@Suppress("LongParameterList")
private fun WallpaperThumbnailItem(
    wallpaper: Wallpaper,
    defaultWallpaper: Wallpaper,
    loadWallpaperResource: (Wallpaper) -> Bitmap?,
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

    val bitmap = loadWallpaperResource(wallpaper)
    // Completely avoid drawing the item if a bitmap cannot be loaded and is required
    if (bitmap == null && wallpaper != defaultWallpaper) return
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
        if (bitmap != null) {
            Image(
                bitmap = bitmap.asImageBitmap(),
                contentScale = ContentScale.FillBounds,
                contentDescription = stringResource(
                    R.string.wallpapers_item_name_content_description, wallpaper.name
                ),
                modifier = Modifier.fillMaxSize(),
            )
        }
    }
}

@Composable
private fun WallpaperLogoSwitch(
    checked: Boolean,
    onCheckedChange: (Boolean) -> Unit
) {
    Column(
        modifier = Modifier.padding(horizontal = 12.dp, vertical = 16.dp),
    ) {
        Row(
            horizontalArrangement = Arrangement.SpaceBetween,
            modifier = Modifier.fillMaxWidth()
        ) {
            Text(
                text = stringResource(R.string.wallpaper_tap_to_change_switch_label_1),
                color = FirefoxTheme.colors.textPrimary,
                fontSize = 18.sp,
                modifier = Modifier.padding(start = 4.dp)
            )
            Switch(
                checked = checked,
                onCheckedChange = onCheckedChange,
                colors = SwitchDefaults.colors(
                    checkedThumbColor = FirefoxTheme.colors.formSelected,
                    checkedTrackColor = FirefoxTheme.colors.formSurface,
                    uncheckedTrackColor = FirefoxTheme.colors.formSurface
                )
            )
        }
    }
}

@Preview
@Composable
private fun WallpaperThumbnailsPreview() {
    FirefoxTheme {
        val context = LocalContext.current
        val wallpaperManager = context.components.wallpaperManager

        WallpaperSettings(
            defaultWallpaper = WallpaperManager.defaultWallpaper,
            loadWallpaperResource = {
                wallpaperManager.loadSavedWallpaper(context, it)
            },
            wallpapers = wallpaperManager.wallpapers,
            selectedWallpaper = wallpaperManager.currentWallpaper,
            onSelectWallpaper = {},
            onViewWallpaper = {},
            tapLogoSwitchChecked = false,
            onTapLogoSwitchCheckedChange = {}
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
