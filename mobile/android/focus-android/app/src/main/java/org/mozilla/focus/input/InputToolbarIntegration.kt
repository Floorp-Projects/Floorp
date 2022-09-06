/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.input

import androidx.core.content.ContextCompat
import androidx.core.content.res.ResourcesCompat
import mozilla.components.browser.domains.autocomplete.CustomDomainsProvider
import mozilla.components.browser.domains.autocomplete.DomainAutocompleteResult
import mozilla.components.browser.domains.autocomplete.ShippedDomainsProvider
import mozilla.components.browser.toolbar.BrowserToolbar
import mozilla.components.concept.toolbar.Toolbar
import mozilla.components.support.base.feature.LifecycleAwareFeature
import org.mozilla.focus.R
import org.mozilla.focus.ext.settings
import org.mozilla.focus.fragment.UrlInputFragment

class InputToolbarIntegration(
    toolbar: BrowserToolbar,
    fragment: UrlInputFragment,
    shippedDomainsProvider: ShippedDomainsProvider,
    customDomainsProvider: CustomDomainsProvider,
) : LifecycleAwareFeature {
    private val settings = toolbar.context.settings

    private var useShippedDomainProvider: Boolean = false
    private var useCustomDomainProvider: Boolean = false

    init {
        with(toolbar.display) {
            indicators = emptyList()
            hint = fragment.getString(R.string.urlbar_hint)
            colors = toolbar.display.colors.copy(
                hint = ContextCompat.getColor(toolbar.context, R.color.urlBarHintText),
                text = ContextCompat.getColor(toolbar.context, R.color.primaryText),
            )
        }
        toolbar.edit.hint = fragment.getString(R.string.urlbar_hint)
        toolbar.private = true
        toolbar.edit.colors = toolbar.edit.colors.copy(
            hint = ContextCompat.getColor(toolbar.context, R.color.urlBarHintText),
            text = ContextCompat.getColor(toolbar.context, R.color.primaryText),
            clear = ContextCompat.getColor(toolbar.context, R.color.primaryText),
        )

        toolbar.setOnEditListener(
            object : Toolbar.OnEditListener {
                override fun onStartEditing() {
                    fragment.onStartEditing()
                }

                override fun onCancelEditing(): Boolean {
                    fragment.onCancelEditing()
                    return true
                }

                override fun onTextChanged(text: String) {
                    fragment.onTextChange(text)
                }
            },
        )

        toolbar.setOnUrlCommitListener { url ->
            fragment.onCommit(url)
            false
        }

        toolbar.setAutocompleteListener { text, delegate ->
            var result: DomainAutocompleteResult? = null
            if (useCustomDomainProvider) {
                result = customDomainsProvider.getAutocompleteSuggestion(text)
            }

            if (useShippedDomainProvider && result == null) {
                result = shippedDomainsProvider.getAutocompleteSuggestion(text)
            }

            if (result != null) {
                delegate.applyAutocompleteResult(
                    mozilla.components.concept.toolbar.AutocompleteResult(
                        result.input,
                        result.text,
                        result.url,
                        result.source,
                        result.totalItems,
                    ),
                )
            } else {
                delegate.noAutocompleteResult(text)
            }
        }

        // Use the same background for display/edit modes.
        val urlBackground = ResourcesCompat.getDrawable(
            fragment.resources,
            R.drawable.toolbar_url_background,
            fragment.context?.theme,
        )

        toolbar.display.setUrlBackground(urlBackground)
        toolbar.edit.setUrlBackground(urlBackground)
    }

    override fun start() {
        useCustomDomainProvider = settings.shouldAutocompleteFromCustomDomainList()
        useShippedDomainProvider = settings.shouldAutocompleteFromShippedDomainList()
    }

    override fun stop() {
        // Do nothing
    }
}
