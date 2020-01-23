/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.samples.browser.addons

import android.content.Intent
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.Toast
import androidx.fragment.app.Fragment
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import kotlinx.android.synthetic.main.fragment_add_ons.*
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import mozilla.components.feature.addons.Addon
import mozilla.components.feature.addons.AddonManagerException
import mozilla.components.feature.addons.ui.AddonsManagerAdapter
import mozilla.components.feature.addons.ui.AddonsManagerAdapterDelegate
import mozilla.components.feature.addons.ui.PermissionsDialogFragment
import mozilla.components.feature.addons.ui.translatedName
import org.mozilla.samples.browser.R
import org.mozilla.samples.browser.ext.components

/**
 * Fragment use for managing add-ons.
 */
class AddonsFragment : Fragment(), AddonsManagerAdapterDelegate {
    private lateinit var recyclerView: RecyclerView
    private val scope = CoroutineScope(Dispatchers.IO)

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View {
        return inflater.inflate(R.layout.fragment_add_ons, container, false)
    }

    override fun onViewCreated(rootView: View, savedInstanceState: Bundle?) {
        super.onViewCreated(rootView, savedInstanceState)
        bindRecyclerView(rootView)
    }

    override fun onStart() {
        super.onStart()

        this@AddonsFragment.view?.let { view ->
            bindRecyclerView(view)
        }
        addonProgressOverlay.visibility = View.GONE

        findPreviousDialogFragment()?.let { dialog ->
            dialog.onPositiveButtonClicked = onPositiveButtonClicked
        }
    }

    private fun bindRecyclerView(rootView: View) {
        recyclerView = rootView.findViewById(R.id.add_ons_list)
        recyclerView.layoutManager = LinearLayoutManager(requireContext())
        scope.launch {
            try {
                val context = requireContext()
                val addonCollectionProvider = context.components.addonCollectionProvider
                val addons = context.components.addonManager.getAddons()

                scope.launch(Dispatchers.Main) {
                    val adapter = AddonsManagerAdapter(
                        addonCollectionProvider = addonCollectionProvider,
                        addonsManagerDelegate = this@AddonsFragment,
                        addons = addons
                    )
                    recyclerView.adapter = adapter
                }
            } catch (e: AddonManagerException) {
                scope.launch(Dispatchers.Main) {
                    Toast.makeText(
                        activity,
                        R.string.mozac_feature_addons_failed_to_query_add_ons,
                        Toast.LENGTH_SHORT
                    ).show()
                }
            }
        }
    }

    override fun onAddonItemClicked(addon: Addon) {
        val context = requireContext()

        if (addon.isInstalled()) {
            val intent = Intent(context, InstalledAddonDetailsActivity::class.java)
            intent.putExtra("add_on", addon)
            context.startActivity(intent)
        } else {
            val intent = Intent(context, AddonDetailsActivity::class.java)
            intent.putExtra("add_on", addon)
            this.startActivity(intent)
        }
    }

    override fun onInstallAddonButtonClicked(addon: Addon) {
        showPermissionDialog(addon)
    }

    override fun onNotYetSupportedSectionClicked(unsupportedAddons: ArrayList<Addon>) {
        val intent = Intent(context, NotYetSupportedAddonActivity::class.java)
        intent.putExtra("add_ons", unsupportedAddons)
        requireContext().startActivity(intent)
    }

    private fun isAlreadyADialogCreated(): Boolean {
        return findPreviousDialogFragment() != null
    }

    private fun findPreviousDialogFragment(): PermissionsDialogFragment? {
        return fragmentManager?.findFragmentByTag(PERMISSIONS_DIALOG_FRAGMENT_TAG) as? PermissionsDialogFragment
    }

    private fun showPermissionDialog(addon: Addon) {
        val dialog = PermissionsDialogFragment.newInstance(
            addon = addon,
            onPositiveButtonClicked = onPositiveButtonClicked
        )

        if (!isAlreadyADialogCreated() && fragmentManager != null) {
            dialog.show(requireFragmentManager(), PERMISSIONS_DIALOG_FRAGMENT_TAG)
        }
    }

    private val onPositiveButtonClicked: ((Addon) -> Unit) = { addon ->
        addonProgressOverlay.visibility = View.VISIBLE
        requireContext().components.addonManager.installAddon(
            addon,
            onSuccess = {
                Toast.makeText(
                    requireContext(),
                    getString(R.string.mozac_feature_addons_successfully_installed, it.translatedName),
                    Toast.LENGTH_SHORT
                ).show()

                this@AddonsFragment.view?.let { view ->
                    bindRecyclerView(view)
                }

                addonProgressOverlay.visibility = View.GONE
            },
            onError = { _, _ ->
                Toast.makeText(
                    requireContext(),
                    getString(R.string.mozac_feature_addons_failed_to_install, addon.translatedName),
                    Toast.LENGTH_SHORT
                ).show()

                addonProgressOverlay.visibility = View.GONE
            }
        )
    }

    companion object {
        private const val PERMISSIONS_DIALOG_FRAGMENT_TAG = "ADDONS_PERMISSIONS_DIALOG_FRAGMENT"
    }
}
