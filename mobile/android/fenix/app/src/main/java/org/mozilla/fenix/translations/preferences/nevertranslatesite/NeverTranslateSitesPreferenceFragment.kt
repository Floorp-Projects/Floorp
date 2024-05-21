/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.translations.preferences.nevertranslatesite

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.compose.ui.platform.ComposeView
import androidx.fragment.app.Fragment
import androidx.navigation.fragment.findNavController
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.translate.TranslationError
import mozilla.components.lib.state.ext.observeAsComposableState
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.requireComponents
import org.mozilla.fenix.ext.showToolbar
import org.mozilla.fenix.theme.FirefoxTheme

/**
 * A fragment displaying never translate site items list.
 */
class NeverTranslateSitesPreferenceFragment : Fragment() {

    private val browserStore: BrowserStore by lazy { requireComponents.core.store }

    override fun onResume() {
        super.onResume()
        showToolbar(getString(R.string.never_translate_site_toolbar_title_preference))
    }

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?,
    ): View = ComposeView(requireContext()).apply {
        setContent {
            FirefoxTheme {
                val neverTranslateSites = browserStore.observeAsComposableState { state ->
                    state.translationEngine.neverTranslateSites
                }.value

                val engineError = browserStore.observeAsComposableState { state ->
                    state.translationEngine.engineError
                }.value

                val couldNotLoadNeverTranslateSites =
                    engineError as? TranslationError.CouldNotLoadNeverTranslateSites

                NeverTranslateSitesPreference(
                    neverTranslateSitesListPreferences = neverTranslateSites,
                    hasNeverTranslateSitesError = couldNotLoadNeverTranslateSites != null ||
                        neverTranslateSites == null,
                    onItemClick = {
                        findNavController().navigate(
                            NeverTranslateSitesPreferenceFragmentDirections
                                .actionNeverTranslateSitePreferenceToNeverTranslateSiteDialogPreference(
                                    neverTranslateSiteUrl = it,
                                ),
                        )
                    },
                )
            }
        }
    }
}
