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
import kotlinx.android.synthetic.main.overlay_add_on_progress.view.*
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
import mozilla.components.feature.addons.ui.translatedName
import org.mozilla.samples.browser.R
import org.mozilla.samples.browser.ext.components
import java.util.concurrent.CancellationException

/**
 * Fragment use for managing add-ons.
 */
@Suppress("TooManyFunctions")
class AddonsFragment : Fragment(), AddonsManagerAdapterDelegate {
    private lateinit var recyclerView: RecyclerView
    private val scope = CoroutineScope(Dispatchers.IO)
    private var adapter: AddonsManagerAdapter? = null
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
                    if (adapter == null) {
                        adapter = AddonsManagerAdapter(
                                addonCollectionProvider = addonCollectionProvider,
                                addonsManagerDelegate = this@AddonsFragment,
                                addons = addons
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

    override fun onNotYetSupportedSectionClicked(unsupportedAddons: List<Addon>) {
        val intent = Intent(context, NotYetSupportedAddonActivity::class.java)
        intent.putExtra("add_ons", ArrayList(unsupportedAddons))
        requireContext().startActivity(intent)
    }

    private fun isAlreadyADialogCreated(): Boolean {
        return findPreviousDialogFragment() != null
    }

    private fun findPreviousDialogFragment(): PermissionsDialogFragment? {
        return fragmentManager?.findFragmentByTag(PERMISSIONS_DIALOG_FRAGMENT_TAG) as? PermissionsDialogFragment
    }

    private fun showPermissionDialog(addon: Addon) {
        if (isInstallationInProgress) {
            return
        }

        val dialog = PermissionsDialogFragment.newInstance(
            addon = addon,
            onPositiveButtonClicked = onPositiveButtonClicked
        )

        if (!isAlreadyADialogCreated() && fragmentManager != null) {
            dialog.show(requireFragmentManager(), PERMISSIONS_DIALOG_FRAGMENT_TAG)
        }
    }

    private fun showInstallationDialog(addon: Addon) {
        if (isInstallationInProgress) {
            return
        }
        val addonCollectionProvider = context!!.components.addonCollectionProvider
        val dialog = AddonInstallationDialogFragment.newInstance(
            addon = addon,
            addonCollectionProvider = addonCollectionProvider,
            onConfirmButtonClicked = { _, allowInPrivateBrowsing ->
                if (allowInPrivateBrowsing) {
                    requireContext().components.addonManager.setAddonAllowedInPrivateBrowsing(
                        addon,
                        allowInPrivateBrowsing
                    )
                }
            }
        )

        if (!isAlreadyADialogCreated() && fragmentManager != null) {
            dialog.show(requireFragmentManager(), INSTALLATION_DIALOG_FRAGMENT_TAG)
        }
    }

    private val onPositiveButtonClicked: ((Addon) -> Unit) = { addon ->
        addonProgressOverlay.visibility = View.VISIBLE
        isInstallationInProgress = true

        val installOperation = requireContext().components.addonManager.installAddon(
            addon,
            onSuccess = {
                adapter?.updateAddon(it)
                addonProgressOverlay.visibility = View.GONE
                isInstallationInProgress = false
                showInstallationDialog(it)
            },
            onError = { _, e ->
                // No need to display an error message if installation was cancelled by the user.
                if (e !is CancellationException) {
                    Toast.makeText(
                        requireContext(), getString(
                        R.string.mozac_feature_addons_failed_to_install,
                        addon.translatedName
                ),
                        Toast.LENGTH_SHORT
                    ).show()
                }

                addonProgressOverlay.visibility = View.GONE
                isInstallationInProgress = false
            }
        )

        addonProgressOverlay.cancel_button.setOnClickListener {
            MainScope().launch {
                // Hide the installation progress overlay once cancellation is successful.
                if (installOperation.cancel().await()) {
                    addonProgressOverlay.visibility = View.GONE
                }
            }
        }
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
