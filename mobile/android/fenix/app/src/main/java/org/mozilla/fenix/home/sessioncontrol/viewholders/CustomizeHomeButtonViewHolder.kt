/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.sessioncontrol.viewholders

import android.view.View
import androidx.compose.foundation.isSystemInDarkTheme
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.material.Button
import androidx.compose.material.ButtonDefaults
import androidx.compose.material.ButtonDefaults.outlinedButtonColors
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.ComposeView
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.Font
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.lifecycle.LifecycleOwner
import mozilla.components.ui.colors.PhotonColors
import org.mozilla.fenix.R
import org.mozilla.fenix.compose.ComposeViewHolder
import org.mozilla.fenix.home.sessioncontrol.CustomizeHomeIteractor
import org.mozilla.fenix.theme.FirefoxTheme

class CustomizeHomeButtonViewHolder(
    composeView: ComposeView,
    viewLifecycleOwner: LifecycleOwner,
    private val interactor: CustomizeHomeIteractor
) : ComposeViewHolder(composeView, viewLifecycleOwner) {

    companion object {
        val LAYOUT_ID = View.generateViewId()
    }

    @Composable
    override fun Content() {
        Column {
            Spacer(modifier = Modifier.height(68.dp))

            CustomizeHomeButton(
                onButtonClick = { interactor.openCustomizeHomePage() }
            )
        }
    }
}

/**
 * A "Customize homepage" button.
 *
 * @param onButtonClick Invoked when the user clicks on the button.
 */
@Composable
fun CustomizeHomeButton(
    onButtonClick: () -> Unit
) {
    val backgroundColor = when (isSystemInDarkTheme()) {
        true -> PhotonColors.DarkGrey50
        false -> PhotonColors.LightGrey40
    }

    Button(
        onClick = { onButtonClick() },
        modifier = Modifier
            .padding(horizontal = 16.dp)
            .fillMaxWidth()
            .height(36.dp),
        elevation = ButtonDefaults.elevation(defaultElevation = 0.dp, pressedElevation = 0.dp),
        colors = outlinedButtonColors(
            backgroundColor = backgroundColor,
            contentColor = FirefoxTheme.colors.textPrimary
        )
    ) {
        Text(
            text = stringResource(R.string.browser_menu_customize_home_1),
            fontSize = 14.sp,
            fontFamily = FontFamily(Font(R.font.metropolis_semibold)),
            letterSpacing = 0.5.sp,
            lineHeight = 16.sp,
            maxLines = 1
        )
    }
}

@Composable
@Preview
fun CustomizeHomeButtonPreview() {
    CustomizeHomeButton(onButtonClick = {})
}
