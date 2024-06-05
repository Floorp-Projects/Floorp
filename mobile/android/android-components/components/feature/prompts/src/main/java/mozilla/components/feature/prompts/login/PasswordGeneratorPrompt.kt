/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.prompts.login

import android.content.Context
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.material.Icon
import androidx.compose.material.MaterialTheme
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.core.content.withStyledAttributes
import mozilla.components.feature.prompts.R
import mozilla.components.feature.prompts.identitycredential.previews.DialogPreviewMaterialTheme
import mozilla.components.support.ktx.android.content.getColorFromAttr

private val Context.primaryColor: Color
    get() = Color(getColorFromAttr(android.R.attr.textColorPrimary))

private val Context.accentColor: Color
    get() {
        var color = Color.Unspecified
        withStyledAttributes(null, R.styleable.LoginSelectBar) {
            val resId = getResourceId(R.styleable.LoginSelectBar_mozacLoginSelectHeaderTextStyle, 0)
            if (resId > 0) {
                withStyledAttributes(resId, intArrayOf(android.R.attr.textColor)) {
                    color = Color(getColor(0, android.graphics.Color.BLACK))
                }
            }
        }

        return color
    }

/**
 * Colors used to theme [PasswordGeneratorPrompt]
 *
 * @param primary The color used for the text in [PasswordGeneratorPrompt].
 * @param header The color used for the header in [PasswordGeneratorPrompt].
 */
data class PasswordGeneratorPromptColors(
    val primary: Color,
    val header: Color,
) {
    constructor(context: Context) : this(
        primary = context.primaryColor,
        header = context.accentColor,
    )
}

/**
 * The password generator prompt
 *
 * @param onGeneratedPasswordPromptClick A callback invoked when the user clicks on the prompt.
 * @param modifier The [Modifier] used for this view.
 * @param colors The [PasswordGeneratorPromptColors] used for this view.
 */
@Composable
fun PasswordGeneratorPrompt(
    onGeneratedPasswordPromptClick: () -> Unit,
    modifier: Modifier = Modifier,
    colors: PasswordGeneratorPromptColors,
) {
    Row(
        modifier = modifier
            .clickable { onGeneratedPasswordPromptClick() }
            .fillMaxWidth()
            .height(48.dp)
            .padding(horizontal = 16.dp),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.Start,
    ) {
        Icon(
            painter = painterResource(id = R.drawable.mozac_ic_login_24),
            contentDescription = null,
            tint = colors.header,
        )

        Spacer(Modifier.width(24.dp))

        Text(
            text = stringResource(id = R.string.mozac_feature_prompts_suggest_strong_password_2),
            color = colors.header,
            fontSize = 16.sp,
            style = MaterialTheme.typography.subtitle2,
        )
    }
}

@Preview
@Composable
private fun PasswordGeneratorPromptPreview() {
    DialogPreviewMaterialTheme {
        PasswordGeneratorPrompt(
            onGeneratedPasswordPromptClick = {},
            colors = PasswordGeneratorPromptColors(
                primary = MaterialTheme.colors.primary,
                header = MaterialTheme.colors.onBackground,
            ),
            modifier = Modifier.background(Color.White),
        )
    }
}
