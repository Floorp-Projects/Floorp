package org.mozilla.fenix.search.awesomebar

/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import android.view.View
import androidx.core.content.ContextCompat
import androidx.navigation.Navigation
import io.reactivex.Observer
import kotlinx.android.synthetic.main.fragment_search.*
import mozilla.components.browser.search.SearchEngine
import org.jetbrains.anko.textColor
import org.mozilla.fenix.DefaultThemeManager
import org.mozilla.fenix.R
import org.mozilla.fenix.search.SearchFragmentDirections

class ShortcutEngineManager(
    private val awesomeBarUIView: AwesomeBarUIView,
    private val actionEmitter: Observer<AwesomeBarAction>,
    private val setShortcutEngine: (newEngine: SearchEngine) -> Unit,
    private val showSuggestionProviders: () -> Unit,
    private val showSearchSuggestionProvider: () -> Unit
) {

    var shortcutsEnginePickerProvider: ShortcutsSuggestionProvider? = null
    val context = awesomeBarUIView.containerView?.context!!

    fun updateSelectedEngineIfNecessary(newState: AwesomeBarState) {
        if (engineDidChange(newState)) {
            newState.suggestionEngine?.let { newEngine ->
                setShortcutEngine(newEngine)
            }
        }
    }

    fun updateEnginePickerVisibilityIfNecessary(newState: AwesomeBarState) {
        if (shouldUpdateShortcutEnginePickerVisibility(newState)) {
            if (newState.showShortcutEnginePicker) {
                showShortcutEnginePicker()
                updateSearchWithVisibility(true)
            } else {
                hideShortcutEnginePicker()
                updateSearchWithVisibility(false)
                newState.suggestionEngine?.also { showSearchSuggestionProvider() } ?: showSuggestionProviders()
            }
        }
    }

    fun selectShortcutEngine(engine: SearchEngine) {
        actionEmitter.onNext(AwesomeBarAction.SearchShortcutEngineSelected(engine))
    }

    fun selectShortcutEngineSettings() {
        val directions = SearchFragmentDirections.actionSearchFragmentToSearchEngineFragment()
        Navigation.findNavController(awesomeBarUIView.view).navigate(directions)
    }

    private fun engineDidChange(newState: AwesomeBarState): Boolean {
        return awesomeBarUIView.state?.suggestionEngine != newState.suggestionEngine
    }

    private fun shouldUpdateShortcutEnginePickerVisibility(newState: AwesomeBarState): Boolean {
        return awesomeBarUIView.state?.showShortcutEnginePicker != newState.showShortcutEnginePicker
    }

    private fun showShortcutEnginePicker() {
        with(context) {
            awesomeBarUIView.search_shortcuts_button.background = getDrawable(R.drawable.search_pill_background)

            awesomeBarUIView.search_shortcuts_button.compoundDrawables[0].setTint(ContextCompat.getColor(this,
                DefaultThemeManager.resolveAttribute(R.attr.pillWrapperBackground, this)))

            awesomeBarUIView.search_shortcuts_button.textColor = ContextCompat.getColor(this,
                DefaultThemeManager.resolveAttribute(R.attr.pillWrapperBackground, this))

            awesomeBarUIView.view.removeAllProviders()
            awesomeBarUIView.view.addProviders(shortcutsEnginePickerProvider!!)
        }
    }

    private fun hideShortcutEnginePicker() {
        with(context) {
            awesomeBarUIView.search_shortcuts_button.setBackgroundColor(ContextCompat.getColor(this,
                DefaultThemeManager.resolveAttribute(R.attr.pillWrapperBackground, this)))

            awesomeBarUIView.search_shortcuts_button.compoundDrawables[0].setTint(ContextCompat.getColor(this,
                DefaultThemeManager.resolveAttribute(R.attr.searchShortcutsTextColor, this)))

            awesomeBarUIView.search_shortcuts_button.textColor = ContextCompat.getColor(this,
                DefaultThemeManager.resolveAttribute(R.attr.searchShortcutsTextColor, this))

            awesomeBarUIView.view.removeProviders(shortcutsEnginePickerProvider!!)
        }
    }

    private fun updateSearchWithVisibility(visible: Boolean) {
        awesomeBarUIView.search_with_shortcuts.visibility = if (visible) View.VISIBLE else View.GONE
    }
}
