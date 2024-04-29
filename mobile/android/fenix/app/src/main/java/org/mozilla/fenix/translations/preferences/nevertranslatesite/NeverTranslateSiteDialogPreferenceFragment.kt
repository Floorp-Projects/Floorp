/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.translations.preferences.nevertranslatesite

import android.app.Dialog
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.compose.ui.platform.ComposeView
import androidx.fragment.app.DialogFragment
import androidx.navigation.fragment.findNavController
import androidx.navigation.fragment.navArgs
import mozilla.components.browser.state.action.TranslationsAction
import mozilla.components.browser.state.store.BrowserStore
import org.mozilla.fenix.ext.requireComponents
import org.mozilla.fenix.theme.FirefoxTheme

/**
 * A dialog fragment displaying never translate site item.
 */
class NeverTranslateSiteDialogPreferenceFragment : DialogFragment() {

    private val args by navArgs<NeverTranslateSiteDialogPreferenceFragmentArgs>()
    private val browserStore: BrowserStore by lazy { requireComponents.core.store }

    override fun onCreateDialog(savedInstanceState: Bundle?): Dialog =
        super.onCreateDialog(savedInstanceState).apply {
            setOnShowListener {
                setCanceledOnTouchOutside(false)
            }
        }

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?,
    ): View = ComposeView(requireContext()).apply {
        setContent {
            FirefoxTheme {
                NeverTranslateSiteDialogPreference(
                    websiteUrl = args.neverTranslateSiteUrl,
                    onConfirmDelete = {
                        browserStore.dispatch(
                            TranslationsAction.RemoveNeverTranslateSiteAction(
                                origin = args.neverTranslateSiteUrl,
                            ),
                        )
                        findNavController().popBackStack()
                    },
                    onCancel = {
                        findNavController().popBackStack()
                    },
                )
            }
        }
    }
}
