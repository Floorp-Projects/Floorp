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
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.aspectRatio
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.Scaffold
import androidx.compose.material.Snackbar
import androidx.compose.material.SnackbarDuration
import androidx.compose.material.SnackbarHost
import androidx.compose.material.Surface
import androidx.compose.material.Switch
import androidx.compose.material.SwitchDefaults
import androidx.compose.material.Text
import androidx.compose.material.rememberScaffoldState
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.platform.LocalConfiguration
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import kotlinx.coroutines.launch
import org.mozilla.fenix.R
import org.mozilla.fenix.compose.button.TextButton
import org.mozilla.fenix.theme.FirefoxTheme
import org.mozilla.fenix.theme.Theme
import org.mozilla.fenix.wallpapers.Wallpaper

/**
 * The screen for controlling settings around Wallpapers. When a new wallpaper is selected,
 * a snackbar will be displayed.
 *
 * @param wallpapers Wallpapers to add to grid.
 * @param selectedWallpaper The currently selected wallpaper.
 * @param defaultWallpaper The default wallpaper
 * @param loadWallpaperResource Callback to handle loading a wallpaper bitmap. Only optional in the default case.
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
    loadWallpaperResource: suspend (Wallpaper) -> Bitmap?,
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
        Column(modifier = Modifier.verticalScroll(rememberScrollState())) {
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
                color = FirefoxTheme.colors.textOnColorPrimary,
                overflow = TextOverflow.Ellipsis,
                maxLines = 2,
                style = FirefoxTheme.typography.headline7
            )
        },
        action = {
            TextButton(
                text = stringResource(R.string.wallpaper_updated_snackbar_action),
                onClick = onViewWallpaper,
                modifier = Modifier.padding(all = 8.dp),
                textColor = FirefoxTheme.colors.textOnColorPrimary,
            )
        },
    )
}

/**
 * A grid of selectable wallpaper thumbnails.
 *
 * @param wallpapers Wallpapers to add to grid.
 * @param defaultWallpaper The default wallpaper
 * @param loadWallpaperResource Callback to handle loading a wallpaper bitmap. Only optional in the default case.
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
    loadWallpaperResource: suspend (Wallpaper) -> Bitmap?,
    selectedWallpaper: Wallpaper,
    numColumns: Int = 3,
    onSelectWallpaper: (Wallpaper) -> Unit,
) {
    Surface(color = FirefoxTheme.colors.layer2) {
        Column(modifier = Modifier.padding(vertical = 30.dp, horizontal = 20.dp)) {
            val numRows = (wallpapers.size + numColumns - 1) / numColumns
            for (rowIndex in 0 until numRows) {
                Row {
                    for (columnIndex in 0 until numColumns) {
                        val itemIndex = rowIndex * numColumns + columnIndex
                        if (itemIndex < wallpapers.size) {
                            Box(
                                modifier = Modifier.weight(1f, fill = true).padding(4.dp),
                            ) {
                                WallpaperThumbnailItem(
                                    wallpaper = wallpapers[itemIndex],
                                    defaultWallpaper = defaultWallpaper,
                                    loadWallpaperResource = loadWallpaperResource,
                                    isSelected = selectedWallpaper == wallpapers[itemIndex],
                                    onSelect = onSelectWallpaper
                                )
                            }
                        } else {
                            Spacer(Modifier.weight(1f))
                        }
                    }
                }
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
    loadWallpaperResource: suspend (Wallpaper) -> Bitmap?,
    isSelected: Boolean,
    aspectRatio: Float = 1.1f,
    onSelect: (Wallpaper) -> Unit
) {
    var bitmap: Bitmap? by remember { mutableStateOf(null) }
    LaunchedEffect(LocalConfiguration.current.orientation) {
        bitmap = loadWallpaperResource(wallpaper)
    }
    val thumbnailShape = RoundedCornerShape(8.dp)
    val border = if (isSelected) {
        Modifier.border(
            BorderStroke(width = 2.dp, color = FirefoxTheme.colors.borderAccent),
            thumbnailShape
        )
    } else {
        Modifier
    }

    // Completely avoid drawing the item if a bitmap cannot be loaded and is required
    if (bitmap == null && wallpaper != defaultWallpaper) return

    Surface(
        elevation = 4.dp,
        shape = thumbnailShape,
        color = FirefoxTheme.colors.layer1,
        modifier = Modifier
            .fillMaxWidth()
            .aspectRatio(aspectRatio)
            .then(border)
            .clickable { onSelect(wallpaper) }
    ) {
        bitmap?.let {
            Image(
                bitmap = it.asImageBitmap(),
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
@Suppress("MagicNumber")
private fun WallpaperLogoSwitch(
    checked: Boolean,
    onCheckedChange: (Boolean) -> Unit
) {
    Row(
        modifier = Modifier
            .padding(horizontal = 16.dp, vertical = 16.dp)
            .fillMaxWidth(),
        horizontalArrangement = Arrangement.SpaceBetween
    ) {
        Text(
            text = stringResource(R.string.wallpaper_tap_to_change_switch_label_1),
            color = FirefoxTheme.colors.textPrimary,
            fontSize = 18.sp,
            modifier = Modifier
                .weight(0.8f)
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

@Preview
@Composable
private fun WallpaperThumbnailsPreview() {
    FirefoxTheme(theme = Theme.getTheme()) {

        WallpaperSettings(
            defaultWallpaper = Wallpaper.Default,
            loadWallpaperResource = { null },
            wallpapers = listOf(Wallpaper.Default),
            selectedWallpaper = Wallpaper.Default,
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
    FirefoxTheme(theme = Theme.getTheme()) {
        WallpaperSnackbar(
            onViewWallpaper = {}
        )
    }
}
