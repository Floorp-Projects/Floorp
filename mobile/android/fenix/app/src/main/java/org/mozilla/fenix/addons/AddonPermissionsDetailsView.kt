/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.addons

import android.net.Uri
import android.view.View
import androidx.annotation.StringRes
import androidx.core.net.toUri
import androidx.recyclerview.widget.LinearLayoutManager
import kotlinx.android.extensions.LayoutContainer
import kotlinx.android.synthetic.main.fragment_add_on_permissions.*
import mozilla.components.feature.addons.Addon
import mozilla.components.feature.addons.ui.AddonPermissionsAdapter
import org.mozilla.fenix.R
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
class AddonPermissionsDetailsView(
    override val containerView: View,
    private val interactor: AddonPermissionsDetailsInteractor
) : LayoutContainer {

    fun bind(addon: Addon) {
        bindPermissions(addon)
        bindLearnMore()
    }

    private fun bindPermissions(addon: Addon) {
        add_ons_permissions.apply {
            layoutManager = LinearLayoutManager(context)
            val sortedPermissions = addon.translatePermissions().map {
                @StringRes val stringId = it
                context.getString(stringId)
            }.sorted()
            adapter = AddonPermissionsAdapter(
                sortedPermissions,
                style = AddonPermissionsAdapter.Style(
                    ThemeManager.resolveAttribute(R.attr.primaryText, context)
                )
            )
        }
    }

    private fun bindLearnMore() {
        learn_more_label.setOnClickListener {
            interactor.openWebsite(LEARN_MORE_URL.toUri())
        }
    }

    private companion object {
        const val LEARN_MORE_URL =
            "https://support.mozilla.org/kb/permission-request-messages-firefox-extensions"
    }
}
