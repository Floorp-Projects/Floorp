/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.addons

import android.net.Uri
import androidx.core.net.toUri
import androidx.recyclerview.widget.LinearLayoutManager
import mozilla.components.feature.addons.Addon
import mozilla.components.feature.addons.ui.AddonPermissionsAdapter
import org.mozilla.fenix.R
import org.mozilla.fenix.databinding.FragmentAddOnPermissionsBinding
import org.mozilla.fenix.theme.ThemeManager

interface AddonPermissionsDetailsInteractor {

    /**
     * Open the given siteUrl in the browser.
     */
    fun openWebsite(addonSiteUrl: Uri)
}

/**
 * Shows the permission details of an add-on.
 */
class AddonPermissionDetailsBindingDelegate(
    val binding: FragmentAddOnPermissionsBinding,
    private val interactor: AddonPermissionsDetailsInteractor
) {

    fun bind(addon: Addon) {
        bindPermissions(addon)
        bindLearnMore()
    }

    private fun bindPermissions(addon: Addon) {
        binding.addOnsPermissions.apply {
            layoutManager = LinearLayoutManager(context)
            val sortedPermissions = addon.translatePermissions(context).sorted()
            adapter = AddonPermissionsAdapter(
                sortedPermissions,
                style = AddonPermissionsAdapter.Style(
                    ThemeManager.resolveAttribute(R.attr.primaryText, context)
                )
            )
        }
    }

    private fun bindLearnMore() {
        binding.learnMoreLabel.setOnClickListener {
            interactor.openWebsite(LEARN_MORE_URL.toUri())
        }
    }

    private companion object {
        const val LEARN_MORE_URL =
            "https://support.mozilla.org/kb/permission-request-messages-firefox-extensions"
    }
}
