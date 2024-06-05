/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.prompts.login

import androidx.compose.material.ContentAlpha
import androidx.compose.material.MaterialTheme
import androidx.compose.runtime.Composable
import androidx.compose.ui.graphics.Color

/**
 * Creates a [PasswordGeneratorDialogColors] that represents the default colors used in an
 * Password Generator bottom sheet dialog.
 *
 * @param title The text color for the title of the dialog.
 * @param description The text color for the description.
 * @param background The background color of the dialog.
 * @param confirmButton The color of the confirmation dialog.
 * @param passwordBox The color of the box that contains the generated password.
 * @param boxBorder The border color of the box that contains the generated password.
 */
data class PasswordGeneratorDialogColors(
    val title: Color,
    val description: Color,
    val background: Color,
    val confirmButton: Color,
    val passwordBox: Color,
    val boxBorder: Color,
) {

    companion object {

        /**
         * @see [PasswordGeneratorDialogColors]
         */
        @Composable
        fun default(
            title: Color = MaterialTheme.colors.onBackground,
            description: Color = MaterialTheme.colors.onBackground.copy(
                alpha = ContentAlpha.medium,
            ),
            background: Color = MaterialTheme.colors.primary,
            confirmButton: Color = MaterialTheme.colors.primary,
            passwordBox: Color = MaterialTheme.colors.primary,
            boxBorder: Color = MaterialTheme.colors.primary,
        ) = PasswordGeneratorDialogColors(
            title = title,
            description = description,
            background = background,
            confirmButton = confirmButton,
            passwordBox = passwordBox,
            boxBorder = boxBorder,
        )

        /**
         * Creates a provider that provides the default [PasswordGeneratorDialogColors]
         */
        fun defaultProvider() = PasswordGeneratorDialogColorsProvider { default() }
    }
}

/**
 * An [PasswordGeneratorDialogColorsProvider] implementation can provide an [PasswordGeneratorDialogColors]
 */
fun interface PasswordGeneratorDialogColorsProvider {

    /**
     * Provides [PasswordGeneratorDialogColors]
     */
    @Composable
    fun provideColors(): PasswordGeneratorDialogColors
}
