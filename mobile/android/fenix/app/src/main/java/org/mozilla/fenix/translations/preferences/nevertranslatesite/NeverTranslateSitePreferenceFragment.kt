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
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.showToolbar
import org.mozilla.fenix.theme.FirefoxTheme

/**
 * A fragment displaying never translate site items list.
 */
class NeverTranslateSitePreferenceFragment : Fragment() {
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
                NeverTranslateSitePreference(
                    neverTranslateSiteListPreferences = getNeverTranslateListItemsPreference(),
                    onItemClick = {
                        findNavController().navigate(
                            NeverTranslateSitePreferenceFragmentDirections
                                .actionNeverTranslateSitePreferenceFragmentToNeverTranslateSiteDialogPreferenceFragment(
                                    it.websiteUrl,
                                ),
                        )
                    },
                )
            }
        }
    }
}
