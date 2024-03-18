/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.samples.browser.awesomebar

import android.content.Context
import android.util.AttributeSet
import androidx.compose.runtime.Composable
import androidx.compose.runtime.mutableStateOf
import androidx.compose.ui.platform.AbstractComposeView
import mozilla.components.compose.browser.awesomebar.AwesomeBar
import mozilla.components.concept.awesomebar.AwesomeBar

/**
 * This wrapper wraps the `AwesomeBar()` composable and exposes it as a `View` and `concept-awesomebar`
 * implementation.
 */
class AwesomeBarWrapper @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = 0,
) : AbstractComposeView(context, attrs, defStyleAttr), AwesomeBar {
    private val providers = mutableStateOf(emptyList<AwesomeBar.SuggestionProvider>())
    private val text = mutableStateOf("")
    private var onEditSuggestionListener: ((String) -> Unit)? = null
    private var onStopListener: (() -> Unit)? = null

    @Composable
    override fun Content() {
        AwesomeBar(
            text = text.value,
            providers = providers.value,
            onSuggestionClicked = { suggestion ->
                suggestion.onSuggestionClicked?.invoke()
                onStopListener?.invoke()
            },
            onAutoComplete = { suggestion ->
                onEditSuggestionListener?.invoke(suggestion.editSuggestion!!)
            },
        )
    }

    override fun addProviders(vararg providers: AwesomeBar.SuggestionProvider) {
        val newProviders = this.providers.value.toMutableList()
        newProviders.addAll(providers)
        this.providers.value = newProviders
    }

    override fun containsProvider(provider: AwesomeBar.SuggestionProvider): Boolean {
        return providers.value.any { current -> current.id == provider.id }
    }

    override fun onInputChanged(text: String) {
        this.text.value = text
    }

    override fun removeAllProviders() {
        providers.value = emptyList()
    }

    override fun removeProviders(vararg providers: AwesomeBar.SuggestionProvider) {
        val newProviders = this.providers.value.toMutableList()
        newProviders.removeAll(providers)
        this.providers.value = newProviders
    }

    override fun setOnEditSuggestionListener(listener: (String) -> Unit) {
        onEditSuggestionListener = listener
    }

    override fun setOnStopListener(listener: () -> Unit) {
        onStopListener = listener
    }
}
