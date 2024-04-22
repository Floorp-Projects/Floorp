/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.microsurvey.ui

import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.Card
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.RectangleShape
import androidx.compose.ui.unit.dp
import org.mozilla.fenix.compose.annotation.LightDarkPreview
import org.mozilla.fenix.theme.FirefoxTheme

/**
 * A scaffold for micro-survey UI that implements the basic layout structure with
 * [content].
 *
 * @param content The content of micro-survey.
 */
@Composable
fun MicroSurveyScaffold(
    content: @Composable () -> Unit,
) {
    var isOpen by remember { mutableStateOf(false) }
    val cardShape = if (isOpen) {
        RoundedCornerShape(topStart = 16.dp, topEnd = 16.dp)
    } else {
        RectangleShape
    }
    val height = if (isOpen) {
        600.dp
    } else {
        200.dp
    }

    Card(
        shape = cardShape,
        backgroundColor = FirefoxTheme.colors.actionQuarternary,
        modifier = Modifier
            .fillMaxWidth()
            .clickable(
                onClick = { isOpen = !isOpen },
            ),
    ) {
        Column(modifier = Modifier.height(height)) {
            content()
        }
    }
}

@LightDarkPreview
@Composable
private fun MicroSurveyScaffoldPreview() {
    FirefoxTheme {
        MicroSurveyScaffold {}
    }
}
