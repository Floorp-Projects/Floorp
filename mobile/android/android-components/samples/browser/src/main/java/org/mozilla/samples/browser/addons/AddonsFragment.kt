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
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.MainScope
import kotlinx.coroutines.launch
import mozilla.components.feature.addons.Addon
import mozilla.components.feature.addons.AddonManagerException
import mozilla.components.feature.addons.ui.AddonInstallationDialogFragment
import mozilla.components.feature.addons.ui.AddonsManagerAdapter
import mozilla.components.feature.addons.ui.AddonsManagerAdapterDelegate
import mozilla.components.feature.addons.ui.PermissionsDialogFragment
import mozilla.components.feature.addons.ui.translateName
import org.mozilla.samples.browser.R
import org.mozilla.samples.browser.databinding.FragmentAddOnsBinding
import org.mozilla.samples.browser.databinding.OverlayAddOnProgressBinding
import org.mozilla.samples.browser.ext.components
import java.util.concurrent.CancellationException

/**
 * Fragment use for managing add-ons.
 */
class AddonsFragment : Fragment(), AddonsManagerAdapterDelegate {
    private lateinit var recyclerView: RecyclerView
    private val scope = CoroutineScope(Dispatchers.IO)
    private var adapter: AddonsManagerAdapter? = null

    private var _binding: FragmentAddOnsBinding? = null
    private val binding get() = _binding!!

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?,
    ): View {
        _binding = FragmentAddOnsBinding.inflate(inflater, container, false)
        return binding.root
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

        findPreviousPermissionDialogFragment()?.let { dialog ->
            dialog.onPositiveButtonClicked = onConfirmPermissionButtonClicked
        }

        findPreviousInstallationDialogFragment()?.let { dialog ->
            dialog.onConfirmButtonClicked = onConfirmInstallationButtonClicked
            dialog.addonCollectionProvider = requireContext().components.addonCollectionProvider
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

                val style = AddonsManagerAdapter.Style(
                    dividerColor = R.color.browser_actions_divider_color,
                    dividerHeight = R.dimen.mozac_browser_menu_item_divider_height,
                )

                scope.launch(Dispatchers.Main) {
                    if (adapter == null) {
                        adapter = AddonsManagerAdapter(
                            addonCollectionProvider = addonCollectionProvider,
                            addonsManagerDelegate = this@AddonsFragment,
                            addons = addons,
                            style = style,
                        )
                        recyclerView.adapter = adapter
                    } else {
                        adapter?.updateAddons(addons)
                    }
                }
            } catch (e: AddonManagerException) {
                scope.launch(Dispatchers.Main) {
                    Toast.makeText(
                        activity,
                        R.string.mozac_feature_addons_failed_to_query_add_ons,
                        Toast.LENGTH_SHORT,
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

    override fun onNotYetSupportedSectionClicked(unsupportedAddons: List<Addon>) {
        val intent = Intent(context, NotYetSupportedAddonActivity::class.java)
        intent.putExtra("add_ons", ArrayList(unsupportedAddons))
        requireContext().startActivity(intent)
    }

    private fun isAlreadyADialogCreated(): Boolean {
        return findPreviousPermissionDialogFragment() != null && findPreviousInstallationDialogFragment() != null
    }

    private fun findPreviousPermissionDialogFragment(): PermissionsDialogFragment? {
        return parentFragmentManager.findFragmentByTag(
            PERMISSIONS_DIALOG_FRAGMENT_TAG,
        ) as? PermissionsDialogFragment
    }

    private fun findPreviousInstallationDialogFragment(): AddonInstallationDialogFragment? {
        return parentFragmentManager.findFragmentByTag(
            INSTALLATION_DIALOG_FRAGMENT_TAG,
        ) as? AddonInstallationDialogFragment
    }

    private fun showPermissionDialog(addon: Addon) {
        if (isInstallationInProgress) {
            return
        }

        val dialog = PermissionsDialogFragment.newInstance(
            addon = addon,
            onPositiveButtonClicked = onConfirmPermissionButtonClicked,
        )

        if (!isAlreadyADialogCreated() && isAdded) {
            dialog.show(parentFragmentManager, PERMISSIONS_DIALOG_FRAGMENT_TAG)
        }
    }

    private fun showInstallationDialog(addon: Addon) {
        if (isInstallationInProgress) {
            return
        }
        val addonCollectionProvider = requireContext().components.addonCollectionProvider
        val dialog = AddonInstallationDialogFragment.newInstance(
            addon = addon,
            addonCollectionProvider = addonCollectionProvider,
            onConfirmButtonClicked = onConfirmInstallationButtonClicked,
        )

        if (!isAlreadyADialogCreated() && isAdded) {
            dialog.show(parentFragmentManager, INSTALLATION_DIALOG_FRAGMENT_TAG)
        }
    }

    private val onConfirmInstallationButtonClicked: ((Addon, Boolean) -> Unit) = { addon, allowInPrivateBrowsing ->
        if (allowInPrivateBrowsing) {
            requireContext().components.addonManager.setAddonAllowedInPrivateBrowsing(
                addon,
                allowInPrivateBrowsing,
            )
        }
    }

    private val onConfirmPermissionButtonClicked: ((Addon) -> Unit) = { addon ->
        val includedBinding = OverlayAddOnProgressBinding.bind(binding.addonProgressOverlay.addonProgressOverlay)

        includedBinding.root.visibility = View.VISIBLE
        isInstallationInProgress = true

        val installOperation = requireContext().components.addonManager.installAddon(
            addon,
            onSuccess = { installedAddon ->
                context?.let {
                    adapter?.updateAddon(installedAddon)
                    includedBinding.root.visibility = View.GONE
                    isInstallationInProgress = false
                    showInstallationDialog(installedAddon)
                }
            },
            onError = { _, e ->
                // No need to display an error message if installation was cancelled by the user.
                if (e !is CancellationException) {
                    Toast.makeText(
                        requireContext(),
                        getString(
                            R.string.mozac_feature_addons_failed_to_install,
                            addon.translateName(requireContext()),
                        ),
                        Toast.LENGTH_SHORT,
                    ).show()
                }

                includedBinding.root.visibility = View.GONE
                isInstallationInProgress = false
            },
        )

        includedBinding.cancelButton.setOnClickListener {
            MainScope().launch {
                // Hide the installation progress overlay once cancellation is successful.
                if (installOperation.cancel().await()) {
                    includedBinding.root.visibility = View.GONE
                }
            }
        }
    }

    override fun onDestroyView() {
        super.onDestroyView()
        _binding = null
    }

    /**
     * Whether or not an add-on installation is in progress.
     */
    private var isInstallationInProgress = false

    companion object {
        private const val PERMISSIONS_DIALOG_FRAGMENT_TAG = "ADDONS_PERMISSIONS_DIALOG_FRAGMENT"
        private const val INSTALLATION_DIALOG_FRAGMENT_TAG = "ADDONS_INSTALLATION_DIALOG_FRAGMENT"
    }
}
