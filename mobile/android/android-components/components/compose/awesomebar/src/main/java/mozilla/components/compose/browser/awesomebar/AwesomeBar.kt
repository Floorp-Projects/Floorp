/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.compose.browser.awesomebar

import android.graphics.Bitmap
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material.ContentAlpha
import androidx.compose.material.LocalContentAlpha
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.CompositionLocalProvider
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import mozilla.components.concept.awesomebar.AwesomeBar

/**
 * An awesome bar displaying suggestions from the list of provided [AwesomeBar.SuggestionProvider]s.
 */
@Composable
fun AwesomeBar(
    text: String,
    providers: List<AwesomeBar.SuggestionProvider>,
    onSuggestionClicked: (AwesomeBar.Suggestion) -> Unit
) {
    Column(
        modifier = Modifier
            .fillMaxWidth()
            .background(Color.White)
    ) {
        val suggestions = remember { mutableStateOf(emptyList<AwesomeBar.Suggestion>()) }

        LaunchedEffect(text) {
            suggestions.value = providers.flatMap { provider -> provider.onInputChanged(text) }
        }

        Suggestions(suggestions.value, onSuggestionClicked)
    }
}

@Composable
private fun Suggestions(
    suggestions: List<AwesomeBar.Suggestion>,
    onSuggestionClicked: (AwesomeBar.Suggestion) -> Unit
) {
    LazyColumn {
        items(suggestions) { suggestion ->
            Suggestion(suggestion, onSuggestionClicked)
        }
    }
}

@Composable
private fun Suggestion(
    suggestion: AwesomeBar.Suggestion,
    onSuggestionClicked: (AwesomeBar.Suggestion) -> Unit
) {
    Row(
        modifier = Modifier
            .clickable { onSuggestionClicked(suggestion) }
            .padding(start = 16.dp, top = 8.dp, bottom = 8.dp)
    ) {
        SuggestionIcon(
            icon = suggestion.icon,
            modifier = Modifier.align(Alignment.CenterVertically)
        )
        SuggestionTitleAndDescription(
            title = suggestion.title,
            description = suggestion.description,
        )
    }
}

@Composable
private fun SuggestionTitleAndDescription(
    title: String?,
    description: String?,
) {
    Column {
        Text(
            text = title ?: "",
            fontSize = 15.sp,
            maxLines = 1,
            modifier = Modifier
                .fillMaxWidth()
                .padding(start = 8.dp, end = 8.dp)
        )
        if (description?.isNotEmpty() == true) {
            CompositionLocalProvider(LocalContentAlpha provides ContentAlpha.medium) {
                Text(
                    text = description,
                    fontSize = 12.sp,
                    maxLines = 1,
                    modifier = Modifier
                        .fillMaxWidth()
                        .padding(start = 8.dp, end = 8.dp)
                )
            }
        }
    }
}

@Composable
private fun SuggestionIcon(
    icon: Bitmap?,
    modifier: Modifier
) {
    if (icon == null) {
        return
    }

    Image(
        icon.asImageBitmap(),
        contentDescription = null,
        modifier = modifier
            .width(24.dp)
            .height(24.dp)
    )
}
