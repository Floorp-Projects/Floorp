/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.prompts.identitycredential.previews

import androidx.compose.foundation.isSystemInDarkTheme
import androidx.compose.material.MaterialTheme
import androidx.compose.material.darkColors
import androidx.compose.material.lightColors
import androidx.compose.runtime.Composable
import mozilla.components.ui.colors.PhotonColors

@Composable
internal fun DialogPreviewMaterialTheme(content: @Composable () -> Unit) {
    val colors = if (!isSystemInDarkTheme()) {
        lightColors()
    } else {
        darkColors(background = PhotonColors.DarkGrey30)
    }
    MaterialTheme(colors = colors) {
        content()
    }
}
