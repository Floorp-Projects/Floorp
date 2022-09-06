/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.locale.screen

import android.os.Bundle
import androidx.compose.foundation.lazy.rememberLazyListState
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.MutableState
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import kotlinx.coroutines.launch
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.lib.state.ext.observeAsComposableState
import mozilla.components.support.locale.LocaleUseCases
import org.mozilla.focus.R
import org.mozilla.focus.ext.components
import org.mozilla.focus.ext.requireComponents
import org.mozilla.focus.settings.BaseComposeFragment
import org.mozilla.focus.state.AppAction
import org.mozilla.focus.state.Screen

class LanguageFragment : BaseComposeFragment() {
    private lateinit var browserStore: BrowserStore
    private lateinit var localeUseCases: LocaleUseCases
    private lateinit var languageScreenStore: LanguageScreenStore
    private lateinit var defaultLanguageScreenInteractor: DefaultLanguageScreenInteractor

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        browserStore = requireContext().components.store
        localeUseCases = LocaleUseCases(browserStore)
        languageScreenStore = LanguageScreenStore(
            LanguageScreenState(),
            listOf(
                LanguageMiddleware(
                    activity = requireActivity(),
                    localeUseCase = localeUseCases,
                ),
            ),
        )
        defaultLanguageScreenInteractor = DefaultLanguageScreenInteractor(
            languageScreenStore = languageScreenStore,
        )
    }

    override val titleRes: Int
        get() = R.string.preference_language

    override fun onNavigateUp(): () -> Unit {
        return {
            requireComponents.appStore.dispatch(
                AppAction.OpenSettings(Screen.Settings.Page.General),
            )
        }
    }

    @Composable
    override fun Content() {
        val languagesList = languageScreenStore
            .observeAsComposableState { state -> state.languageList }.value
        val languageSelected = languageScreenStore.observeAsComposableState { state ->
            state.selectedLanguage
        }.value
        if (languageSelected != null && languagesList != null) {
            Languages(languageSelected = languageSelected, languages = languagesList)
        }
    }

    private fun createLanguageListItem(
        languages: List<Language>,
        state: MutableState<String>,
    ): List<LanguageListItem> {
        val languageListItems = ArrayList<LanguageListItem>()
        languages.forEach { language ->
            if (language.tag == LanguageStorage.LOCALE_SYSTEM_DEFAULT) {
                language.displayName = context?.getString(R.string.preference_language_systemdefault)
            }
            val languageListItem = LanguageListItem(
                language = language,
                onClick = {
                    state.value = language.tag
                    defaultLanguageScreenInteractor.handleLanguageSelected(language)
                },
            )
            languageListItems.add(languageListItem)
        }
        return languageListItems
    }

    @Composable
    private fun Languages(languageSelected: Language, languages: List<Language>) {
        val listState = rememberLazyListState()
        val coroutineScope = rememberCoroutineScope()
        LaunchedEffect(languageSelected.index) {
            coroutineScope.launch {
                languageSelected.let { listState.scrollToItem(it.index) }
            }
        }
        val state = remember {
            mutableStateOf(languageSelected.tag)
        }
        LanguagesList(
            languageListItems = createLanguageListItem(
                languages = languages,
                state = state,
            ),
            state = state,
            listState = listState,
        )
    }
}
