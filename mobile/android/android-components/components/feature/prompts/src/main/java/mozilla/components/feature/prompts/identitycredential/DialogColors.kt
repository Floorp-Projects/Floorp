/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.prompts.identitycredential

import androidx.compose.material.ContentAlpha
import androidx.compose.material.MaterialTheme
import androidx.compose.runtime.Composable
import androidx.compose.ui.graphics.Color

/**
 * Represents the colors used by the dialogs.
 */
data class DialogColors(
    val title: Color,
    val description: Color,
) {

    companion object {

        /**
         * Creates an [DialogColors] that represents the default colors used in an
         * IdentityCredential dialog.
         *
         * @param title The text color for the title of a suggestion.
         * @param description The text color for the description of a suggestion.
         */
        @Composable
        fun default(
            title: Color = MaterialTheme.colors.onBackground,
            description: Color = MaterialTheme.colors.onBackground.copy(
                alpha = ContentAlpha.medium,
            ),
        ) = DialogColors(
            title,
            description,
        )

        /**
         * Creates a provider that provides the default [DialogColors]
         */
        fun defaultProvider() = DialogColorsProvider { default() }
    }
}

/**
 * An [DialogColorsProvider] implementation can provide an [DialogColors]
 */
fun interface DialogColorsProvider {

    /**
     * Provides [DialogColors]
     */
    @Composable
    fun provideColors(): DialogColors
}
