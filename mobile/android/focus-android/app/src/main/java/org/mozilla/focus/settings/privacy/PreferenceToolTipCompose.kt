/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.settings.privacy

import android.content.Context
import android.util.AttributeSet
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.wrapContentHeight
import androidx.compose.foundation.layout.wrapContentSize
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.Icon
import androidx.compose.material.IconButton
import androidx.compose.material.Text
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Close
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.res.colorResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.preference.Preference
import androidx.preference.PreferenceViewHolder
import org.mozilla.focus.R
import org.mozilla.focus.databinding.FocusPreferenceComposeLayoutBinding
import org.mozilla.focus.ext.settings
import org.mozilla.focus.ui.theme.FocusTheme
import org.mozilla.focus.ui.theme.focusColors

class PreferenceToolTipCompose(context: Context, attrs: AttributeSet?) :
    Preference(context, attrs) {

    override fun onBindViewHolder(holder: PreferenceViewHolder) {
        super.onBindViewHolder(holder)
        val binding = FocusPreferenceComposeLayoutBinding.bind(holder.itemView)
        binding.toolTipContent.setContent {
            ToolTipContent(
                onDismissButton = {
                    context.settings.shouldShowPrivacySecuritySettingsToolTip = false
                    preferenceManager.preferenceScreen.removePreference(this)
                },
            )
        }
    }
}

@Composable
@Preview
private fun ToolTipContentPreview() {
    ToolTipContent {
    }
}

/**
 * Displays a tool tip for Privacy Security Settings screen.
 *
 * @param onDismissButton Callback when the "x" Button from tool tip is pressed.
 */
@Composable
fun ToolTipContent(onDismissButton: () -> Unit) {
    FocusTheme {
        Column(
            modifier = Modifier
                .fillMaxWidth()
                .wrapContentSize(Alignment.Center),
        ) {
            Box(
                modifier = Modifier
                    .fillMaxWidth()
                    .wrapContentHeight()
                    .padding(16.dp)
                    .clip(RoundedCornerShape(10.dp))
                    .background(
                        shape = RoundedCornerShape(10.dp),
                        brush = Brush.linearGradient(
                            colors = listOf(
                                colorResource(R.color.cfr_pop_up_shape_end_color),
                                colorResource(R.color.cfr_pop_up_shape_start_color),
                            ),
                            end = Offset(0f, Float.POSITIVE_INFINITY),
                            start = Offset(Float.POSITIVE_INFINITY, 0f),
                        ),
                    ),
                contentAlignment = Alignment.CenterStart,
            ) {
                Column(
                    modifier = Modifier
                        .wrapContentHeight()
                        .padding(16.dp),
                    verticalArrangement = Arrangement.spacedBy(
                        10.dp,
                    ),
                ) {
                    Text(
                        text = stringResource(R.string.tool_tip_title),
                        fontSize = 16.sp,
                        letterSpacing = 0.5.sp,
                        color = focusColors.privacySecuritySettingsToolTip,
                        fontWeight = FontWeight.Bold,
                        lineHeight = 20.sp,
                    )
                    Text(
                        text = stringResource(R.string.tool_tip_message),
                        fontSize = 14.sp,
                        color = focusColors.privacySecuritySettingsToolTip,
                        lineHeight = 20.sp,
                    )
                }
                IconButton(
                    onClick = onDismissButton,
                    modifier = Modifier.align(
                        Alignment.TopEnd,
                    ),
                ) {
                    Icon(
                        Icons.Filled.Close,
                        contentDescription = stringResource(R.string.tool_tip_dismiss_button_content_description),
                        tint = focusColors.privacySecuritySettingsToolTip,
                    )
                }
            }
        }
    }
}
