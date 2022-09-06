/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.locale.screen

import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.layout.wrapContentHeight
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.LazyListState
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.lazy.rememberLazyListState
import androidx.compose.material.RadioButton
import androidx.compose.material.RadioButtonDefaults
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.MutableState
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.RectangleShape
import androidx.compose.ui.res.colorResource
import androidx.compose.ui.text.AnnotatedString
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import kotlinx.coroutines.launch
import org.mozilla.focus.R
import org.mozilla.focus.ui.theme.FocusTheme
import org.mozilla.focus.ui.theme.focusColors

private fun getFakeLanguages(): List<LanguageListItem> {
    return mutableListOf<LanguageListItem>().apply {
        var index = 0
        add(LanguageListItem(Language("Română", "ro", index++), onClick = {}))
        add(LanguageListItem(Language("Slovenčina", "sk", index++), onClick = {}))
        add(LanguageListItem(Language("Português (Brasil)", "pt-BR", index++), onClick = {}))
        add(LanguageListItem(Language("Nederlands", "nl", index++), onClick = {}))
        add(LanguageListItem(Language("Magyar", "hu", index++), onClick = {}))
        add(LanguageListItem(Language("Lietuvių", "lt", ++index), onClick = {}))
    }
}

@Composable
@Preview
private fun LanguagesListComposablePreview() {
    FocusTheme {
        val listState = rememberLazyListState()
        val coroutineScope = rememberCoroutineScope()
        LaunchedEffect(0) {
            coroutineScope.launch {
                listState.scrollToItem(0)
            }
        }
        val state = remember {
            mutableStateOf("ro")
        }
        LanguagesList(
            languageListItems = getFakeLanguages(),
            state = state,
            listState = listState,
        )
    }
}

/**
 * Displays a list of Languages in a listView
 *
 * @param languageListItems The list of Languages items to be displayed.
 * @param state the current Selected Language
 * @param listState scrolls to the current selected Language
 */
@Composable
fun LanguagesList(
    languageListItems: List<LanguageListItem>,
    state: MutableState<String>,
    listState: LazyListState,
) {
    FocusTheme {
        LazyColumn(
            modifier = Modifier.background(colorResource(R.color.settings_background), shape = RectangleShape),
            state = listState,
            contentPadding = PaddingValues(horizontal = 12.dp),
        ) {
            items(languageListItems) { item ->
                LanguageNameAndTagItem(
                    language = item.language,
                    isSelected = state.value == item.language.tag,
                    onClick = item.onClick,
                )
            }
        }
    }
}

@Composable
fun LanguageNameAndTagItem(
    language: Language,
    isSelected: Boolean,
    onClick: (String) -> Unit,
) {
    Row(
        Modifier
            .fillMaxWidth()
            .wrapContentHeight(),
        horizontalArrangement = Arrangement.Start,
        verticalAlignment = Alignment.CenterVertically,
    ) {
        LanguageRadioButton(
            language = language,
            isSelected = isSelected,
            onClick = onClick,
        )
        Spacer(modifier = Modifier.width(18.dp))
        language.displayName?.let {
            LanguageDisplayName(
                language = language,
                onClick = onClick,
            )
        }
    }
}

/**
 * Displays a single language radiobutton
 *
 * @param language of the item
 * @param isSelected check or uncheck the radioButton if the language is selected or not
 * @param onClick Callback when the user taps on Language
 */
@Composable
private fun LanguageRadioButton(
    language: Language,
    isSelected: Boolean,
    onClick: (String) -> Unit,
) {
    RadioButton(
        selected = isSelected,
        colors = RadioButtonDefaults.colors(selectedColor = focusColors.radioButtonSelected),
        onClick = {
            onClick(language.tag)
        },
    )
}

/**
 * Displays a single language Text
 *
 * @param language of the item to be display in the textView
 * @param onClick Callback when the user taps on Language text , the same like on the radioButton
 */
@Composable
private fun LanguageDisplayName(language: Language, onClick: (String) -> Unit) {
    Text(
        text = AnnotatedString(language.displayName!!),
        style = TextStyle(
            fontSize = 20.sp,
        ),
        modifier = Modifier
            .padding(10.dp)
            .clickable { onClick(language.tag) },
    )
}
