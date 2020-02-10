/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.addons

import android.os.Bundle
import android.view.View
import androidx.core.view.isVisible
import androidx.fragment.app.Fragment
import androidx.lifecycle.lifecycleScope
import androidx.navigation.Navigation
import androidx.recyclerview.widget.LinearLayoutManager
import kotlinx.android.synthetic.main.fragment_add_ons_management.*
import kotlinx.android.synthetic.main.fragment_add_ons_management.view.*
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Dispatchers.IO
import kotlinx.coroutines.launch
import kotlinx.coroutines.flow.collect
import kotlinx.coroutines.CoroutineScope
import mozilla.components.lib.state.ext.flowScoped
import mozilla.components.support.ktx.kotlinx.coroutines.flow.ifChanged
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.cancel
import mozilla.components.feature.addons.Addon
import mozilla.components.feature.addons.AddonManagerException
import mozilla.components.feature.addons.ui.AddonsManagerAdapter
import mozilla.components.feature.addons.ui.AddonsManagerAdapterDelegate
import mozilla.components.feature.addons.ui.PermissionsDialogFragment
import mozilla.components.feature.addons.ui.translatedName
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.showToolbar

/**
 * Fragment use for managing add-ons.
 */
@Suppress("TooManyFunctions")
class AddonsManagementFragment : Fragment(R.layout.fragment_add_ons_management),
    AddonsManagerAdapterDelegate {
    /**
     * Whether or not an add-on installation is in progress.
     */
    private var isInstallationInProgress = false
    private var scope: CoroutineScope? = null

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        bindRecyclerView(view)
    }

    override fun onResume() {
        super.onResume()
        showToolbar(getString(R.string.preferences_addons))
    }

    @UseExperimental(ExperimentalCoroutinesApi::class)
    override fun onStart() {
        super.onStart()
        findPreviousDialogFragment()?.let { dialog ->
            dialog.onPositiveButtonClicked = onPositiveButtonClicked
        }

        scope = requireContext().components.core.store.flowScoped { flow ->
            flow.ifChanged { it.extensions }
                .collect { _ ->
                    view?.let {
                        bindRecyclerView(it)
                    }
                }
        }
    }

    override fun onStop() {
        super.onStop()
        scope?.cancel()
    }

    override fun onAddonItemClicked(addon: Addon) {
        if (addon.isInstalled()) {
            showInstalledAddonDetailsFragment(addon)
        } else {
            showDetailsFragment(addon)
        }
    }

    override fun onInstallAddonButtonClicked(addon: Addon) {
        showPermissionDialog(addon)
    }

    override fun onNotYetSupportedSectionClicked(unsupportedAddons: ArrayList<Addon>) {
        showNotYetSupportedAddonFragment(unsupportedAddons)
    }

    private fun bindRecyclerView(view: View) {
        val recyclerView = view.add_ons_list
        recyclerView.layoutManager = LinearLayoutManager(requireContext())
        lifecycleScope.launch(IO) {
            try {
                val addons = requireContext().components.addonManager.getAddons()
                lifecycleScope.launch(Dispatchers.Main) {
                    runIfFragmentIsAttached {
                        val adapter = AddonsManagerAdapter(
                            requireContext().components.addonCollectionProvider,
                            this@AddonsManagementFragment,
                            addons
                        )
                        view.add_ons_progress_bar.isVisible = false
                        view.add_ons_empty_message.isVisible = false

                        recyclerView.adapter = adapter
                    }
                }
            } catch (e: AddonManagerException) {
                lifecycleScope.launch(Dispatchers.Main) {
                    runIfFragmentIsAttached {
                        showSnackBar(view, getString(R.string.mozac_feature_addons_failed_to_query_add_ons))
                        view.add_ons_progress_bar.isVisible = false
                        view.add_ons_empty_message.isVisible = true
                    }
                }
            }
        }
    }

    private fun showInstalledAddonDetailsFragment(addon: Addon) {
        val directions =
            AddonsManagementFragmentDirections.actionAddonsManagementFragmentToInstalledAddonDetails(
                addon
            )
        Navigation.findNavController(requireView()).navigate(directions)
    }

    private fun showDetailsFragment(addon: Addon) {
        val directions =
            AddonsManagementFragmentDirections.actionAddonsManagementFragmentToAddonDetailsFragment(
                addon
            )
        Navigation.findNavController(requireView()).navigate(directions)
    }

    private fun showNotYetSupportedAddonFragment(unsupportedAddons: ArrayList<Addon>) {
        val directions =
            AddonsManagementFragmentDirections.actionAddonsManagementFragmentToNotYetSupportedAddonFragment(
                unsupportedAddons.toTypedArray()
            )
        Navigation.findNavController(requireView()).navigate(directions)
    }

    private fun findPreviousDialogFragment(): PermissionsDialogFragment? {
        return parentFragmentManager.findFragmentByTag(PERMISSIONS_DIALOG_FRAGMENT_TAG) as? PermissionsDialogFragment
    }

    private fun hasExistingPermissionDialogFragment(): Boolean {
        return findPreviousDialogFragment() != null
    }

    private fun showPermissionDialog(addon: Addon) {
        if (!isInstallationInProgress && !hasExistingPermissionDialogFragment()) {
            val dialog = PermissionsDialogFragment.newInstance(
                addon = addon,
                onPositiveButtonClicked = onPositiveButtonClicked
            )
            dialog.show(parentFragmentManager, PERMISSIONS_DIALOG_FRAGMENT_TAG)
        }
    }

    private val onPositiveButtonClicked: ((Addon) -> Unit) = { addon ->
        addonProgressOverlay.visibility = View.VISIBLE
        isInstallationInProgress = true

        requireContext().components.addonManager.installAddon(
            addon,
            onSuccess = {
                this@AddonsManagementFragment.view?.let { view ->
                    showSnackBar(
                        view,
                        getString(
                            R.string.mozac_feature_addons_successfully_installed,
                            it.translatedName
                        )
                    )
                    bindRecyclerView(view)
                    addonProgressOverlay?.visibility = View.GONE
                    isInstallationInProgress = false
                }
            },
            onError = { _, _ ->
                this@AddonsManagementFragment.view?.let { view ->
                    showSnackBar(view, getString(R.string.mozac_feature_addons_failed_to_install, addon.translatedName))
                    addonProgressOverlay?.visibility = View.GONE
                    isInstallationInProgress = false
                }
            }
        )
    }

    companion object {
        private const val PERMISSIONS_DIALOG_FRAGMENT_TAG = "ADDONS_PERMISSIONS_DIALOG_FRAGMENT"
    }
}
