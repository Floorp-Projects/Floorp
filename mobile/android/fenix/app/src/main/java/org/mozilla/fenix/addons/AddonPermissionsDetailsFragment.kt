/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.addons

import android.content.Intent
import android.net.Uri
import android.os.Bundle
import android.view.View
import androidx.annotation.StringRes
import androidx.fragment.app.Fragment
import androidx.navigation.fragment.navArgs
import androidx.recyclerview.widget.LinearLayoutManager
import kotlinx.android.synthetic.main.fragment_add_on_permissions.view.*
import mozilla.components.feature.addons.Addon
import mozilla.components.feature.addons.ui.AddonPermissionsAdapter
import mozilla.components.feature.addons.ui.translate
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.showToolbar
import org.mozilla.fenix.theme.ThemeManager

private const val LEARN_MORE_URL =
    "https://support.mozilla.org/kb/permission-request-messages-firefox-extensions"

/**
 * A fragment to show the permissions of an add-on.
 */
class AddonPermissionsDetailsFragment : Fragment(R.layout.fragment_add_on_permissions), View.OnClickListener {

    private val args by navArgs<AddonPermissionsDetailsFragmentArgs>()

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        showToolbar(args.addon.translatableName.translate())

        bindPermissions(args.addon, view)
        bindLearnMore(view)
    }

    private fun bindPermissions(addon: Addon, view: View) {
        view.add_ons_permissions.apply {
            layoutManager = LinearLayoutManager(requireContext())
            val sortedPermissions = addon.translatePermissions().map {
                @StringRes val stringId = it
                getString(stringId)
            }.sorted()
            adapter = AddonPermissionsAdapter(
                sortedPermissions,
                style = AddonPermissionsAdapter.Style(
                    ThemeManager.resolveAttribute(R.attr.primaryText, requireContext())
                )
            )
        }
    }

    private fun bindLearnMore(view: View) {
        view.learn_more_label.setOnClickListener(this)
    }

    override fun onClick(v: View?) {
        val intent =
            Intent(Intent.ACTION_VIEW).setData(Uri.parse(LEARN_MORE_URL))
        startActivity(intent)
    }
}
