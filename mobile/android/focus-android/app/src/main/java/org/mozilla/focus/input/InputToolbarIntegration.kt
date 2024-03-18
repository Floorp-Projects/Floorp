/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.input

import androidx.annotation.VisibleForTesting
import androidx.appcompat.widget.AppCompatEditText
import androidx.compose.material.Text
import androidx.compose.ui.res.colorResource
import androidx.compose.ui.unit.dp
import androidx.core.content.ContextCompat
import androidx.core.content.res.ResourcesCompat
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.cancel
import kotlinx.coroutines.flow.distinctUntilChanged
import kotlinx.coroutines.flow.mapNotNull
import mozilla.components.browser.domains.autocomplete.CustomDomainsProvider
import mozilla.components.browser.domains.autocomplete.ShippedDomainsProvider
import mozilla.components.browser.toolbar.BrowserToolbar
import mozilla.components.compose.cfr.CFRPopup
import mozilla.components.compose.cfr.CFRPopupProperties
import mozilla.components.concept.toolbar.AutocompleteResult
import mozilla.components.concept.toolbar.Toolbar
import mozilla.components.lib.state.ext.flowScoped
import mozilla.components.support.base.feature.LifecycleAwareFeature
import org.mozilla.focus.R
import org.mozilla.focus.ext.components
import org.mozilla.focus.ext.settings
import org.mozilla.focus.fragment.UrlInputFragment
import org.mozilla.focus.state.AppAction
import org.mozilla.focus.ui.theme.focusTypography

class InputToolbarIntegration(
    private val toolbar: BrowserToolbar,
    private val fragment: UrlInputFragment,
    shippedDomainsProvider: ShippedDomainsProvider,
    customDomainsProvider: CustomDomainsProvider,
) : LifecycleAwareFeature {
    private val settings = toolbar.context.settings

    private var useShippedDomainProvider: Boolean = false
    private var useCustomDomainProvider: Boolean = false

    @VisibleForTesting
    internal var startBrowsingCfrScope: CoroutineScope? = null

    init {
        with(toolbar.display) {
            indicators = emptyList()
            hint = fragment.resources.getString(R.string.urlbar_hint)
            colors = toolbar.display.colors.copy(
                hint = ContextCompat.getColor(toolbar.context, R.color.urlBarHintText),
                text = ContextCompat.getColor(toolbar.context, R.color.primaryText),
            )
        }
        toolbar.edit.hint = fragment.resources.getString(R.string.urlbar_hint)
        toolbar.private = true
        toolbar.edit.colors = toolbar.edit.colors.copy(
            hint = ContextCompat.getColor(toolbar.context, R.color.urlBarHintText),
            text = ContextCompat.getColor(toolbar.context, R.color.primaryText),
            clear = ContextCompat.getColor(toolbar.context, R.color.primaryText),
            suggestionBackground = ContextCompat.getColor(toolbar.context, R.color.autocompleteBackgroundColor),
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
            var result: AutocompleteResult? = null
            if (useCustomDomainProvider) {
                result = customDomainsProvider.getAutocompleteSuggestion(text)
            }

            if (useShippedDomainProvider && result == null) {
                result = shippedDomainsProvider.getAutocompleteSuggestion(text)
            }

            if (result != null) {
                delegate.applyAutocompleteResult(
                    AutocompleteResult(
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
        if (fragment.components?.appStore?.state?.showStartBrowsingTabsCfr == true) {
            observeStartBrowserCfrVisibility()
        }
    }

    @VisibleForTesting
    internal fun observeStartBrowserCfrVisibility() {
        startBrowsingCfrScope = fragment.components?.appStore?.flowScoped { flow ->
            flow.mapNotNull { state -> state.showStartBrowsingTabsCfr }
                .distinctUntilChanged()
                .collect { showStartBrowsingCfr ->
                    if (showStartBrowsingCfr) {
                        CFRPopup(
                            anchor = toolbar.findViewById<AppCompatEditText>(R.id.mozac_browser_toolbar_background),
                            properties = CFRPopupProperties(
                                popupWidth = 256.dp,
                                popupAlignment = CFRPopup.PopupAlignment.BODY_TO_ANCHOR_START,
                                popupBodyColors = listOf(
                                    ContextCompat.getColor(
                                        fragment.requireContext(),
                                        R.color.cfr_pop_up_shape_end_color,
                                    ),
                                    ContextCompat.getColor(
                                        fragment.requireContext(),
                                        R.color.cfr_pop_up_shape_start_color,
                                    ),
                                ),
                                dismissButtonColor = ContextCompat.getColor(
                                    fragment.requireContext(),
                                    R.color.cardview_light_background,
                                ),
                                popupVerticalOffset = 0.dp,
                            ),
                            onDismiss = {
                                onDismissStartBrowsingCfr()
                            },
                            text = {
                                Text(
                                    style = focusTypography.cfrTextStyle,
                                    text = fragment.resources.getString(R.string.cfr_for_start_browsing),
                                    color = colorResource(R.color.cfr_text_color),
                                )
                            },
                        ).apply {
                            show()
                        }
                    }
                }
        }
    }

    private fun onDismissStartBrowsingCfr() {
        fragment.components?.appStore?.dispatch(AppAction.ShowStartBrowsingCfrChange(false))
        fragment.requireContext().settings.shouldShowStartBrowsingCfr = false
    }

    override fun stop() {
        startBrowsingCfrScope?.cancel()
    }
}
