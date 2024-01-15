/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.compose

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.selection.toggleable
import androidx.compose.material.SwitchDefaults
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.compositeOver
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.semantics.clearAndSetSemantics
import androidx.compose.ui.unit.dp
import org.mozilla.fenix.compose.annotation.LightDarkPreview
import org.mozilla.fenix.theme.FirefoxTheme
import androidx.compose.material.Switch as MaterialSwitch

private const val DISABLED_ALPHA = 0.5f

/**
 * UI for a switch with label that can be on or off.
 *
 * @param label Text to be displayed next to the switch.
 * @param description An optional description text below the label.
 * @param checked Whether or not the switch is checked.
 * @param onCheckedChange Invoked when Switch is being clicked, therefore the change of checked
 * state is requested.
 * @param modifier Modifier to be applied to the switch layout.
 * @param enabled Whether the switch is enabled or grayed out.
 */
@Composable
fun SwitchWithLabel(
    label: String,
    description: String? = null,
    checked: Boolean,
    onCheckedChange: ((Boolean) -> Unit),
    modifier: Modifier = Modifier,
    enabled: Boolean = true,
) {
    Row(
        modifier = modifier
            .toggleable(
                value = checked,
                enabled = enabled,
                role = Role.Switch,
                onValueChange = onCheckedChange,
            ),
        horizontalArrangement = Arrangement.spacedBy(16.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Column(
            modifier = Modifier
                .weight(1f),
        ) {
            Text(
                text = label,
                color = if (enabled) {
                    FirefoxTheme.colors.textPrimary
                } else {
                    FirefoxTheme.colors.textDisabled
                },
                style = FirefoxTheme.typography.subtitle1,
            )

            description?.let {
                Text(
                    text = description,
                    color = FirefoxTheme.colors.textSecondary,
                    style = FirefoxTheme.typography.body2,
                )
            }
        }

        Switch(
            modifier = Modifier.clearAndSetSemantics {},
            checked = checked,
            onCheckedChange = onCheckedChange,
            enabled = enabled,
        )
    }
}

/**
 * UI for a switch that can be on or off.
 *
 * @param checked Whether or not the switch is checked.
 * @param onCheckedChange Invoked when Switch is being clicked, therefore the change of checked
 * state is requested.
 * @param modifier Modifier to be applied to the switch layout.
 * @param enabled Whether the switch is enabled or grayed out.
 */
@Composable
private fun Switch(
    checked: Boolean,
    onCheckedChange: ((Boolean) -> Unit),
    modifier: Modifier = Modifier,
    enabled: Boolean = true,
) {
    MaterialSwitch(
        checked = checked,
        onCheckedChange = onCheckedChange,
        modifier = modifier,
        enabled = enabled,
        colors = SwitchDefaults.colors(
            uncheckedThumbColor = FirefoxTheme.colors.formOff,
            uncheckedTrackColor = FirefoxTheme.colors.formSurface,
            checkedThumbColor = FirefoxTheme.colors.formOn,
            checkedTrackColor = FirefoxTheme.colors.formSurface,
            disabledUncheckedThumbColor = FirefoxTheme.colors.formOff
                .copy(alpha = DISABLED_ALPHA)
                .compositeOver(FirefoxTheme.colors.formSurface),
            disabledUncheckedTrackColor = FirefoxTheme.colors.formSurface.copy(alpha = DISABLED_ALPHA),
            disabledCheckedThumbColor = FirefoxTheme.colors.formOn
                .copy(alpha = DISABLED_ALPHA)
                .compositeOver(FirefoxTheme.colors.formSurface),
            disabledCheckedTrackColor = FirefoxTheme.colors.formSurface.copy(alpha = DISABLED_ALPHA),
        ),
    )
}

@LightDarkPreview
@Composable
private fun SwitchWithLabelPreview() {
    FirefoxTheme {
        Column(
            modifier = Modifier
                .background(FirefoxTheme.colors.layer1)
                .padding(16.dp),
            verticalArrangement = Arrangement.spacedBy(8.dp),
        ) {
            Text(
                text = "Enabled",
                style = FirefoxTheme.typography.headline7,
                color = FirefoxTheme.colors.textPrimary,
            )

            Spacer(Modifier.height(8.dp))

            var enabledSwitchState by remember { mutableStateOf(false) }
            SwitchWithLabel(
                label = if (enabledSwitchState) "On" else "Off",
                description = "Description text",
                checked = enabledSwitchState,
                onCheckedChange = { enabledSwitchState = it },
            )

            Text(
                text = "Disabled",
                style = FirefoxTheme.typography.headline7,
                color = FirefoxTheme.colors.textPrimary,
            )

            Spacer(Modifier.height(8.dp))

            var disabledSwitchStateOff by remember { mutableStateOf(false) }
            SwitchWithLabel(
                label = "Off",
                checked = disabledSwitchStateOff,
                enabled = false,
                onCheckedChange = { disabledSwitchStateOff = it },
            )

            var disabledSwitchStateOn by remember { mutableStateOf(true) }
            SwitchWithLabel(
                label = "On",
                checked = disabledSwitchStateOn,
                enabled = false,
                onCheckedChange = { disabledSwitchStateOn = it },
            )

            Text(
                text = "Nested",
                style = FirefoxTheme.typography.headline7,
                color = FirefoxTheme.colors.textPrimary,
            )

            Spacer(Modifier.height(8.dp))

            Row {
                Spacer(Modifier.weight(1f))

                var nestedSwitchState by remember { mutableStateOf(false) }
                SwitchWithLabel(
                    label = "Nested",
                    checked = nestedSwitchState,
                    onCheckedChange = { nestedSwitchState = it },
                    modifier = Modifier.weight(1f),
                )
            }
        }
    }
}
