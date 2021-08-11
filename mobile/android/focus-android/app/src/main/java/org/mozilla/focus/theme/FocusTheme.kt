/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.theme

import androidx.compose.foundation.isSystemInDarkTheme
import androidx.compose.material.Colors
import androidx.compose.material.MaterialTheme
import androidx.compose.material.darkColors
import androidx.compose.runtime.Composable
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.res.colorResource
import org.mozilla.focus.R

/**
 * The theme used for Firefox Focus/Klar for Android.
 */
@Composable
fun FocusTheme(
    darkTheme: Boolean = isSystemInDarkTheme(),
    content: @Composable () -> Unit
) {
    MaterialTheme(
        colors = if (darkTheme) darkColorPalette() else lightColorPalette(),
        content = content
    )
}

@Composable
private fun darkColorPalette(): Colors = darkColors(
    background = colorResource(R.color.photonInk80),
    onBackground = Color.White
)

// Just a copy of the dark palette until we actually have a light theme
@Composable
private fun lightColorPalette(): Colors = darkColorPalette()
