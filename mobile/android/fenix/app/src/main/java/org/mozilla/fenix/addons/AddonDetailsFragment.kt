/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.addons

import android.content.Intent
import android.net.Uri
import android.os.Bundle
import android.view.View
import androidx.fragment.app.Fragment
import androidx.lifecycle.lifecycleScope
import androidx.navigation.fragment.navArgs
import kotlinx.coroutines.Dispatchers.IO
import kotlinx.coroutines.Dispatchers.Main
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import mozilla.components.feature.addons.Addon
import mozilla.components.feature.addons.ui.showInformationDialog
import mozilla.components.feature.addons.ui.translatedName
import mozilla.components.feature.addons.update.DefaultAddonUpdater.UpdateAttemptStorage
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.showToolbar

/**
 * A fragment to show the details of an add-on.
 */
class AddonDetailsFragment : Fragment(R.layout.fragment_add_on_details), AddonDetailsInteractor {

    private val updateAttemptStorage by lazy { UpdateAttemptStorage(requireContext()) }
    private val args by navArgs<AddonDetailsFragmentArgs>()

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        showToolbar(title = args.addon.translatedName)
        AddonDetailsView(view, interactor = this).bind(args.addon)
    }

    override fun openWebsite(addonSiteUrl: Uri) {
        startActivity(Intent(Intent.ACTION_VIEW, addonSiteUrl))
    }

    override fun showUpdaterDialog(addon: Addon) {
        viewLifecycleOwner.lifecycleScope.launch(Main) {
            val updateAttempt = withContext(IO) {
                updateAttemptStorage.findUpdateAttemptBy(addon.id)
            }
            updateAttempt?.showInformationDialog(requireContext())
        }
    }
}
