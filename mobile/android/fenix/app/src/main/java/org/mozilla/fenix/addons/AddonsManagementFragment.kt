/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.addons

import android.content.Context
import android.os.Bundle
import android.view.Gravity
import android.view.View
import android.view.accessibility.AccessibilityEvent
import androidx.annotation.VisibleForTesting
import androidx.core.content.res.ResourcesCompat
import androidx.core.view.isVisible
import androidx.fragment.app.Fragment
import androidx.lifecycle.lifecycleScope
import androidx.navigation.fragment.findNavController
import androidx.navigation.fragment.navArgs
import androidx.recyclerview.widget.LinearLayoutManager
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Dispatchers.IO
import kotlinx.coroutines.launch
import mozilla.components.feature.addons.Addon
import mozilla.components.feature.addons.AddonManagerException
import mozilla.components.feature.addons.ui.AddonInstallationDialogFragment
import mozilla.components.feature.addons.ui.AddonsManagerAdapter
import mozilla.components.feature.addons.ui.PermissionsDialogFragment
import mozilla.components.feature.addons.ui.translateName
import org.mozilla.fenix.BuildConfig
import org.mozilla.fenix.Config
import org.mozilla.fenix.R
import org.mozilla.fenix.components.FenixSnackbar
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.databinding.FragmentAddOnsManagementBinding
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.getRootView
import org.mozilla.fenix.ext.requireComponents
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.ext.showToolbar
import org.mozilla.fenix.ext.runIfFragmentIsAttached
import org.mozilla.fenix.theme.ThemeManager
import java.lang.ref.WeakReference
import java.util.concurrent.CancellationException

/**
 * Fragment use for managing add-ons.
 */
@Suppress("TooManyFunctions", "LargeClass")
class AddonsManagementFragment : Fragment(R.layout.fragment_add_ons_management) {

    private val args by navArgs<AddonsManagementFragmentArgs>()

    private var _binding: FragmentAddOnsManagementBinding? = null
    private val binding get() = _binding!!

    /**
     * Whether or not an add-on installation is in progress.
     */
    private var isInstallationInProgress = false

    private var installExternalAddonComplete: Boolean
        set(value) {
            arguments?.putBoolean(BUNDLE_KEY_INSTALL_EXTERNAL_ADDON_COMPLETE, value)
        }
        get() {
            return arguments?.getBoolean(BUNDLE_KEY_INSTALL_EXTERNAL_ADDON_COMPLETE, false) ?: false
        }

    private var adapter: AddonsManagerAdapter? = null

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        _binding = FragmentAddOnsManagementBinding.bind(view)
        bindRecyclerView()
    }

    override fun onResume() {
        super.onResume()
        showToolbar(getString(R.string.preferences_addons))
    }

    override fun onStart() {
        super.onStart()
        findPreviousDialogFragment()?.let { dialog ->
            dialog.onPositiveButtonClicked = onPositiveButtonClicked
        }
    }

    override fun onDestroyView() {
        super.onDestroyView()
        // letting go of the resources to avoid memory leak.
        adapter = null
        _binding = null
    }

    private fun bindRecyclerView() {
        val managementView = AddonsManagementView(
            navController = findNavController(),
            showPermissionDialog = ::showPermissionDialog
        )

        val recyclerView = binding.addOnsList
        recyclerView.layoutManager = LinearLayoutManager(requireContext())
        val shouldRefresh = adapter != null

        // If the fragment was launched to install an "external" add-on from AMO, we deactivate
        // the cache to get the most up-to-date list of add-ons to match against.
        val allowCache = args.installAddonId == null || installExternalAddonComplete
        lifecycleScope.launch(IO) {
            try {
                val addons = requireContext().components.addonManager.getAddons(allowCache = allowCache)
                // Add-ons that should be excluded in Mozilla Online builds
                val excludedAddonIDs = if (Config.channel.isMozillaOnline &&
                    !BuildConfig.MOZILLA_ONLINE_ADDON_EXCLUSIONS.isNullOrEmpty()
                ) {
                    BuildConfig.MOZILLA_ONLINE_ADDON_EXCLUSIONS.toList()
                } else {
                    emptyList<String>()
                }
                lifecycleScope.launch(Dispatchers.Main) {
                    runIfFragmentIsAttached {
                        if (!shouldRefresh) {
                            adapter = AddonsManagerAdapter(
                                requireContext().components.addonCollectionProvider,
                                managementView,
                                addons,
                                style = createAddonStyle(requireContext()),
                                excludedAddonIDs
                            )
                        }
                        isInstallationInProgress = false
                        binding.addOnsProgressBar.isVisible = false
                        binding.addOnsEmptyMessage.isVisible = false

                        recyclerView.adapter = adapter
                        if (shouldRefresh) {
                            adapter?.updateAddons(addons)
                        }

                        args.installAddonId?.let { addonIn ->
                            if (!installExternalAddonComplete) {
                                installExternalAddon(addons, addonIn)
                            }
                        }
                    }
                }
            } catch (e: AddonManagerException) {
                lifecycleScope.launch(Dispatchers.Main) {
                    runIfFragmentIsAttached {
                        showSnackBar(
                            binding.root,
                            getString(R.string.mozac_feature_addons_failed_to_query_add_ons)
                        )
                        isInstallationInProgress = false
                        binding.addOnsProgressBar.isVisible = false
                        binding.addOnsEmptyMessage.isVisible = true
                    }
                }
            }
        }
    }

    @VisibleForTesting
    internal fun installExternalAddon(supportedAddons: List<Addon>, installAddonId: String) {
        val addonToInstall = supportedAddons.find { it.downloadId == installAddonId }
        if (addonToInstall == null) {
            showErrorSnackBar(getString(R.string.addon_not_supported_error))
        } else {
            if (addonToInstall.isInstalled()) {
                showErrorSnackBar(getString(R.string.addon_already_installed))
            } else {
                showPermissionDialog(addonToInstall)
            }
        }
        installExternalAddonComplete = true
    }

    @VisibleForTesting
    internal fun showErrorSnackBar(text: String) {
        runIfFragmentIsAttached {
            view?.let {
                showSnackBar(it, text, FenixSnackbar.LENGTH_LONG)
            }
        }
    }

    private fun createAddonStyle(context: Context): AddonsManagerAdapter.Style {
        return AddonsManagerAdapter.Style(
            sectionsTextColor = ThemeManager.resolveAttribute(R.attr.primaryText, context),
            addonNameTextColor = ThemeManager.resolveAttribute(R.attr.primaryText, context),
            addonSummaryTextColor = ThemeManager.resolveAttribute(R.attr.secondaryText, context),
            sectionsTypeFace = ResourcesCompat.getFont(context, R.font.metropolis_semibold),
            addonAllowPrivateBrowsingLabelDrawableRes = R.drawable.ic_add_on_private_browsing_label
        )
    }

    private fun findPreviousDialogFragment(): PermissionsDialogFragment? {
        return parentFragmentManager.findFragmentByTag(PERMISSIONS_DIALOG_FRAGMENT_TAG) as? PermissionsDialogFragment
    }

    private fun hasExistingPermissionDialogFragment(): Boolean {
        return findPreviousDialogFragment() != null
    }

    private fun hasExistingAddonInstallationDialogFragment(): Boolean {
        return parentFragmentManager.findFragmentByTag(INSTALLATION_DIALOG_FRAGMENT_TAG)
            as? AddonInstallationDialogFragment != null
    }

    @VisibleForTesting
    internal fun showPermissionDialog(addon: Addon) {
        if (!isInstallationInProgress && !hasExistingPermissionDialogFragment()) {
            val dialog = PermissionsDialogFragment.newInstance(
                addon = addon,
                promptsStyling = PermissionsDialogFragment.PromptsStyling(
                    gravity = Gravity.BOTTOM,
                    shouldWidthMatchParent = true,
                    positiveButtonBackgroundColor = ThemeManager.resolveAttribute(
                        R.attr.accent,
                        requireContext()
                    ),
                    positiveButtonTextColor = ThemeManager.resolveAttribute(
                        R.attr.contrastText,
                        requireContext()
                    ),
                    positiveButtonRadius = (resources.getDimensionPixelSize(R.dimen.tab_corner_radius)).toFloat()
                ),
                onPositiveButtonClicked = onPositiveButtonClicked
            )
            dialog.show(parentFragmentManager, PERMISSIONS_DIALOG_FRAGMENT_TAG)
        }
    }

    private fun showInstallationDialog(addon: Addon) {
        if (!isInstallationInProgress && !hasExistingAddonInstallationDialogFragment()) {
            requireComponents.analytics.metrics.track(Event.AddonInstalled(addon.id))
            val context = requireContext()
            val addonCollectionProvider = context.components.addonCollectionProvider

            // Fragment may not be attached to the context anymore during onConfirmButtonClicked handling,
            // but we still want to be able to process user selection of the 'allowInPrivateBrowsing' pref.
            // This is a best-effort attempt to do so - retain a weak reference to the application context
            // (to avoid a leak), which we attempt to use to access addonManager.
            // See https://github.com/mozilla-mobile/fenix/issues/15816
            val weakApplicationContext: WeakReference<Context> = WeakReference(context)

            val dialog = AddonInstallationDialogFragment.newInstance(
                addon = addon,
                addonCollectionProvider = addonCollectionProvider,
                promptsStyling = AddonInstallationDialogFragment.PromptsStyling(
                    gravity = Gravity.BOTTOM,
                    shouldWidthMatchParent = true,
                    confirmButtonBackgroundColor = ThemeManager.resolveAttribute(
                        R.attr.accent,
                        requireContext()
                    ),
                    confirmButtonTextColor = ThemeManager.resolveAttribute(
                        R.attr.contrastText,
                        requireContext()
                    ),
                    confirmButtonRadius = (resources.getDimensionPixelSize(R.dimen.tab_corner_radius)).toFloat()
                ),
                onConfirmButtonClicked = { _, allowInPrivateBrowsing ->
                    if (allowInPrivateBrowsing) {
                        weakApplicationContext.get()?.components?.addonManager?.setAddonAllowedInPrivateBrowsing(
                            addon,
                            allowInPrivateBrowsing,
                            onSuccess = {
                                runIfFragmentIsAttached {
                                    adapter?.updateAddon(it)
                                }
                            }
                        )
                    }
                }
            )

            dialog.show(parentFragmentManager, INSTALLATION_DIALOG_FRAGMENT_TAG)
        }
    }

    private val onPositiveButtonClicked: ((Addon) -> Unit) = { addon ->
        binding.addonProgressOverlay.overlayCardView.visibility = View.VISIBLE

        if (requireContext().settings().accessibilityServicesEnabled) {
            announceForAccessibility(binding.addonProgressOverlay.addOnsOverlayText.text)
        }

        isInstallationInProgress = true

        val installOperation = requireContext().components.addonManager.installAddon(
            addon,
            onSuccess = {
                runIfFragmentIsAttached {
                    isInstallationInProgress = false
                    adapter?.updateAddon(it)
                    binding.addonProgressOverlay.overlayCardView.visibility = View.GONE
                    showInstallationDialog(it)
                }
            },
            onError = { _, e ->
                this@AddonsManagementFragment.view?.let { view ->
                    // No need to display an error message if installation was cancelled by the user.
                    if (e !is CancellationException) {
                        val rootView = activity?.getRootView() ?: view
                        context?.let {
                            showSnackBar(
                                rootView,
                                getString(
                                    R.string.mozac_feature_addons_failed_to_install,
                                    addon.translateName(it)
                                )
                            )
                        }
                    }
                    binding.addonProgressOverlay.overlayCardView.visibility = View.GONE
                    isInstallationInProgress = false
                }
            }
        )

        binding.addonProgressOverlay.cancelButton.setOnClickListener {
            lifecycleScope.launch(Dispatchers.Main) {
                // Hide the installation progress overlay once cancellation is successful.
                if (installOperation.cancel().await()) {
                    binding.addonProgressOverlay.overlayCardView.visibility = View.GONE
                }
            }
        }
    }

    private fun announceForAccessibility(announcementText: CharSequence) {
        val event = AccessibilityEvent.obtain(
            AccessibilityEvent.TYPE_ANNOUNCEMENT
        )

        binding.addonProgressOverlay.overlayCardView.onInitializeAccessibilityEvent(event)
        event.text.add(announcementText)
        event.contentDescription = null
        binding.addonProgressOverlay.overlayCardView.parent.requestSendAccessibilityEvent(
            binding.addonProgressOverlay.overlayCardView,
            event
        )
    }

    companion object {
        private const val PERMISSIONS_DIALOG_FRAGMENT_TAG = "ADDONS_PERMISSIONS_DIALOG_FRAGMENT"
        private const val INSTALLATION_DIALOG_FRAGMENT_TAG = "ADDONS_INSTALLATION_DIALOG_FRAGMENT"
        private const val BUNDLE_KEY_INSTALL_EXTERNAL_ADDON_COMPLETE = "INSTALL_EXTERNAL_ADDON_COMPLETE"
    }
}
