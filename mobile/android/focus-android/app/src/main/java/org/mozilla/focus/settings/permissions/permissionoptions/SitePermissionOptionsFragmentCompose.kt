/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.settings.permissions.permissionoptions

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.wrapContentHeight
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.selection.selectable
import androidx.compose.material.Button
import androidx.compose.material.ButtonDefaults
import androidx.compose.material.RadioButton
import androidx.compose.material.RadioButtonDefaults
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.MutableState
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.RectangleShape
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.colorResource
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.text.AnnotatedString
import androidx.compose.ui.text.SpanStyle
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.buildAnnotatedString
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.text.withStyle
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import mozilla.components.ui.colors.PhotonColors
import org.mozilla.focus.R
import org.mozilla.focus.settings.permissions.AutoplayOption
import org.mozilla.focus.settings.permissions.SitePermissionOption
import org.mozilla.focus.ui.theme.FocusTheme
import org.mozilla.focus.ui.theme.focusColors

private fun getPermissionOptionsList(): List<SitePermissionOptionListItem> {
    return mutableListOf<SitePermissionOptionListItem>().apply {
        add(SitePermissionOptionListItem(AutoplayOption.AllowAudioVideo(), onClick = {}))
        add(SitePermissionOptionListItem(AutoplayOption.BlockAudioOnly(), onClick = {}))
        add(SitePermissionOptionListItem(AutoplayOption.BlockAudioVideo(), onClick = {}))
    }
}

@Composable
@Preview
private fun PermissionOptionsListComposablePreview() {
    FocusTheme {
        val state = remember {
            mutableStateOf(AutoplayOption.BlockAudioOnly().prefKeyId)
        }
        OptionsPermissionList(
            optionsListItems = getPermissionOptionsList(),
            state = state,
            permissionLabel = "Camera",
            goToPhoneSettings = {},
            componentPermissionBlockedByAndroidVisibility = true,
        )
    }
}

/**
 * Displays a list of Site Permission Options
 *
 * @param optionsListItems The list of Site Permission Options items to be displayed.
 * @param state the current Option
 */
@Composable
fun OptionsPermissionList(
    optionsListItems: List<SitePermissionOptionListItem>,
    state: MutableState<Int>,
    permissionLabel: String?,
    goToPhoneSettings: () -> Unit,
    componentPermissionBlockedByAndroidVisibility: Boolean,
) {
    FocusTheme {
        Column(
            Modifier
                .fillMaxWidth()
                .fillMaxHeight()
                .background(
                    colorResource(R.color.settings_background),
                    shape = RectangleShape,
                ),
        ) {
            LazyColumn(
                contentPadding = PaddingValues(horizontal = 12.dp),
            ) {
                items(optionsListItems) { item ->
                    OptionPermission(
                        sitePermissionOption = item.sitePermissionOption,
                        isSelected = state.value == item.sitePermissionOption.prefKeyId,
                        onClick = item.onClick,
                    )
                }
                item {
                    if (componentPermissionBlockedByAndroidVisibility) {
                        ComponentPermissionBlockedByAndroid(goToPhoneSettings, permissionLabel)
                    }
                }
            }
        }
    }
}

@Composable
private fun OptionPermission(
    sitePermissionOption: SitePermissionOption,
    isSelected: Boolean,
    onClick: (SitePermissionOption) -> Unit,
) {
    Row(
        Modifier
            .fillMaxWidth()
            .wrapContentHeight()
            .selectable(
                selected = isSelected,
                onClick = { onClick(sitePermissionOption) },
                role = Role.RadioButton,
            ),
        horizontalArrangement = Arrangement.Start,
        verticalAlignment = Alignment.CenterVertically,
    ) {
        // When we use the onClick parameter in the selectable modifier we can use the onClick = null
        // in the RadioButton component.
        RadioButton(
            selected = isSelected,
            colors = RadioButtonDefaults.colors(selectedColor = focusColors.radioButtonSelected),
            onClick = null,
        )
        OptionPermissionDisplayName(
            sitePermissionOption = sitePermissionOption,
        )
    }
}

@Composable
private fun OptionPermissionDisplayName(sitePermissionOption: SitePermissionOption) {
    Column(modifier = Modifier.padding(10.dp)) {
        Text(
            textAlign = TextAlign.Start,
            color = focusColors.settingsTextColor,
            text = AnnotatedString(LocalContext.current.resources.getString(sitePermissionOption.titleId)),
            style = TextStyle(
                fontSize = 16.sp,
            ),
            modifier = Modifier
                .padding(start = 8.dp, end = 8.dp),
        )
        sitePermissionOption.summaryId?.let {
            Text(
                textAlign = TextAlign.Start,
                text = AnnotatedString(LocalContext.current.resources.getString(it)),
                color = focusColors.settingsTextSummaryColor,
                style = TextStyle(
                    fontSize = 14.sp,
                ),
                modifier = Modifier
                    .padding(start = 8.dp, end = 8.dp),
            )
        }
    }
}

/**
 * Displays a component if the Site Permission needs user approval from Phone Settings
 * This is needed for Permissions like Camera ,Location, Microphone
 *
 * @param goToPhoneSettings callback when the user press Go to Settings button
 * @param permissionLabel label for the Site Permission
 */

@Composable
private fun ComponentPermissionBlockedByAndroid(goToPhoneSettings: () -> Unit, permissionLabel: String?) {
    Column(
        modifier = Modifier
            .background(colorResource(R.color.settings_background), shape = RectangleShape)
            .fillMaxWidth()
            .padding(top = 16.dp)
            .wrapContentHeight(),
    ) {
        ComponentPermissionBlockedByAndroidText(
            stringRes = R.string.phone_feature_blocked_by_android,
            permissionLabel,
            16.dp,
        )
        ComponentPermissionBlockedByAndroidText(
            stringRes = R.string.phone_feature_blocked_intro,
            permissionLabel,
            16.dp,
        )
        ComponentPermissionBlockedByAndroidText(
            stringRes = R.string.phone_feature_blocked_step_settings,
            permissionLabel,
            8.dp,
        )
        ComponentPermissionBlockedByAndroidText(
            stringRes = R.string.phone_feature_blocked_step_permissions,
            permissionLabel,
            8.dp,
        )
        ComponentPermissionBlockedByAndroidText(
            stringRes = R.string.phone_feature_blocked_step_feature,
            permissionLabel,
        )
        ComponentPermissionBlockedByAndroidButton(
            goToPhoneSettings = goToPhoneSettings,
        )
    }
}

@Composable
private fun ComponentPermissionBlockedByAndroidButton(goToPhoneSettings: () -> Unit) {
    Button(
        onClick = goToPhoneSettings,
        colors = ButtonDefaults.textButtonColors(
            backgroundColor = PhotonColors.LightGrey50,
        ),
        modifier = Modifier
            .padding(16.dp)
            .fillMaxWidth(),
    ) {
        Text(
            color = PhotonColors.Ink20,
            text = AnnotatedString(
                LocalContext.current.resources.getString(
                    R.string.phone_feature_go_to_settings,
                ),
            ),
        )
    }
}

@Composable
private fun ComponentPermissionBlockedByAndroidText(
    stringRes: Int,
    permissionLabel: String?,
    bottomPadding: Dp = 0.dp,
) {
    Text(
        textAlign = TextAlign.Start,
        color = focusColors.settingsTextColor,
        text = LocalContext.current.getString(stringRes, permissionLabel).parseBold(),
        style = TextStyle(
            fontSize = 16.sp,
        ),
        modifier = Modifier.padding(start = 55.dp, end = 16.dp, bottom = bottomPadding),
    )
}

private fun String.parseBold(): AnnotatedString {
    val parts = this.split("<b>", "</b>")
    return buildAnnotatedString {
        var bold = false
        for (part in parts) {
            if (bold) {
                withStyle(style = SpanStyle(fontWeight = FontWeight.Bold)) {
                    append(part)
                }
            } else {
                append(part)
            }
            bold = !bold
        }
    }
}
