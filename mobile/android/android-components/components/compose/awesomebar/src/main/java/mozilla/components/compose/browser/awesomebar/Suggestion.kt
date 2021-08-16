/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.compose.browser.awesomebar

import android.graphics.Bitmap
import androidx.compose.foundation.Image
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.IntrinsicSize
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.material.ContentAlpha
import androidx.compose.material.MaterialTheme
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.ColorFilter
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import mozilla.components.concept.awesomebar.AwesomeBar

@Composable
internal fun Suggestion(
    suggestion: AwesomeBar.Suggestion,
    onSuggestionClicked: (AwesomeBar.Suggestion) -> Unit,
    onAutoComplete: (AwesomeBar.Suggestion) -> Unit
) {
    Row(
        modifier = Modifier
            .clickable { onSuggestionClicked(suggestion) }
            .height(56.dp)
            .padding(start = 16.dp, top = 8.dp, bottom = 8.dp, end = 8.dp)
    ) {
        val icon = suggestion.icon
        if (icon != null) {
            SuggestionIcon(
                icon = icon,
                modifier = Modifier.align(Alignment.CenterVertically)
            )
        }
        SuggestionTitleAndDescription(
            title = suggestion.title,
            description = suggestion.description,
            modifier = Modifier
                .weight(1f)
                .align(Alignment.CenterVertically)
        )
        if (suggestion.editSuggestion != null) {
            AutocompleteButton(
                onAutoComplete = { onAutoComplete(suggestion) },
                modifier = Modifier.align(Alignment.CenterVertically)
            )
        }
    }
}

@Composable
private fun SuggestionTitleAndDescription(
    title: String?,
    description: String?,
    modifier: Modifier = Modifier
) {
    Column(
        modifier = modifier
    ) {
        Text(
            text = title ?: "",
            color = MaterialTheme.colors.onBackground,
            fontSize = 15.sp,
            maxLines = 1,
            modifier = Modifier
                .width(IntrinsicSize.Max)
                .padding(start = 8.dp, end = 8.dp)
        )
        if (description?.isNotEmpty() == true) {
            Text(
                text = description,
                color = MaterialTheme.colors.onBackground.copy(
                    alpha = ContentAlpha.medium
                ),
                fontSize = 12.sp,
                maxLines = 1,
                modifier = Modifier
                    .width(IntrinsicSize.Max)
                    .padding(start = 8.dp, end = 8.dp)
            )
        }
    }
}

@Composable
private fun SuggestionIcon(
    icon: Bitmap,
    modifier: Modifier
) {
    Image(
        icon.asImageBitmap(),
        contentDescription = null,
        modifier = modifier
            .width(24.dp)
            .height(24.dp)
    )
}

@Composable
private fun AutocompleteButton(
    onAutoComplete: () -> Unit,
    modifier: Modifier
) {
    Image(
        painterResource(R.drawable.mozac_ic_edit_suggestion),
        colorFilter = ColorFilter.tint(MaterialTheme.colors.onSurface),
        contentDescription = stringResource(R.string.mozac_browser_awesomebar_edit_suggestion),
        modifier = modifier
            .width(24.dp)
            .height(24.dp)
            .clickable { onAutoComplete() }
    )
}
