/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray

import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import org.mozilla.fenix.R
import org.mozilla.fenix.theme.FirefoxTheme

@Composable
internal fun EmptyTabPage(isPrivate: Boolean) {
    val testTag: String
    val emptyTextId: Int
    if (isPrivate) {
        testTag = TabsTrayTestTag.emptyPrivateTabsList
        emptyTextId = R.string.no_private_tabs_description
    } else {
        testTag = TabsTrayTestTag.emptyNormalTabsList
        emptyTextId = R.string.no_open_tabs_description
    }

    Box(
        modifier = Modifier
            .fillMaxSize()
            .testTag(testTag),
    ) {
        Text(
            text = stringResource(id = emptyTextId),
            modifier = Modifier
                .align(Alignment.TopCenter)
                .padding(top = 80.dp),
            color = FirefoxTheme.colors.textSecondary,
            style = FirefoxTheme.typography.body1,
        )
    }
}
