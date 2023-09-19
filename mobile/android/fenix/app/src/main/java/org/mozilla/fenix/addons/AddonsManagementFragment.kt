/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.addons

import android.content.Context
import android.graphics.Typeface
import android.graphics.fonts.FontStyle.FONT_WEIGHT_MEDIUM
import android.os.Build
import android.os.Bundle
import android.view.View
import android.view.accessibility.AccessibilityEvent
import androidx.annotation.VisibleForTesting
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
import mozilla.components.feature.addons.AddonManager
import mozilla.components.feature.addons.AddonManagerException
import mozilla.components.feature.addons.ui.AddonsManagerAdapter
import mozilla.components.support.base.feature.ViewBoundFeatureWrapper
import org.mozilla.fenix.BrowserDirection
import org.mozilla.fenix.BuildConfig
import org.mozilla.fenix.Config
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.components.FenixSnackbar
import org.mozilla.fenix.databinding.FragmentAddOnsManagementBinding
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.requireComponents
import org.mozilla.fenix.ext.runIfFragmentIsAttached
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.ext.showToolbar
import org.mozilla.fenix.extension.WebExtensionPromptFeature
import org.mozilla.fenix.theme.ThemeManager

/**
 * Fragment use for managing add-ons.
 */
@Suppress("TooManyFunctions", "LargeClass")
class AddonsManagementFragment : Fragment(R.layout.fragment_add_ons_management) {

    private val args by navArgs<AddonsManagementFragmentArgs>()

    private var binding: FragmentAddOnsManagementBinding? = null

    private val webExtensionPromptFeature = ViewBoundFeatureWrapper<WebExtensionPromptFeature>()
    private var addons: List<Addon> = emptyList()

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
        binding = FragmentAddOnsManagementBinding.bind(view)
        bindRecyclerView()
        webExtensionPromptFeature.set(
            feature = WebExtensionPromptFeature(
                store = requireComponents.core.store,
                context = requireContext(),
                fragmentManager = parentFragmentManager,
                onAddonChanged = {
                    runIfFragmentIsAttached {
                        adapter?.updateAddon(it)
                    }
                },
            ),
            owner = this,
            view = view,
        )
    }

    override fun onResume() {
        super.onResume()
        showToolbar(getString(R.string.preferences_addons))
    }

    override fun onDestroyView() {
        super.onDestroyView()
        // letting go of the resources to avoid memory leak.
        adapter = null
        binding = null
    }

    private fun bindRecyclerView() {
        val managementView = AddonsManagementView(
            navController = findNavController(),
            onInstallButtonClicked = ::installAddon,
            onMoreAddonsButtonClicked = ::openAMO,
        )

        val recyclerView = binding?.addOnsList
        recyclerView?.layoutManager = LinearLayoutManager(requireContext())
        val shouldRefresh = adapter != null

        // If the fragment was launched to install an "external" add-on from AMO, we deactivate
        // the cache to get the most up-to-date list of add-ons to match against.
        val allowCache = args.installAddonId == null || installExternalAddonComplete
        lifecycleScope.launch(IO) {
            try {
                addons = requireContext().components.addonManager.getAddons(allowCache)
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
                                requireContext().components.addonsProvider,
                                managementView,
                                addons,
                                style = createAddonStyle(requireContext()),
                                excludedAddonIDs,
                            )
                        }
                        binding?.addOnsProgressBar?.isVisible = false
                        binding?.addOnsEmptyMessage?.isVisible = false

                        recyclerView?.adapter = adapter
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
                        binding?.let {
                            showSnackBar(
                                it.root,
                                getString(R.string.mozac_feature_addons_failed_to_query_add_ons),
                            )
                        }
                        binding?.addOnsProgressBar?.isVisible = false
                        binding?.addOnsEmptyMessage?.isVisible = true
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
                installAddon(addonToInstall)
            }
        }
        installExternalAddonComplete = true
    }

    @VisibleForTesting
    internal fun showErrorSnackBar(text: String, anchorView: View? = this.view) {
        runIfFragmentIsAttached {
            anchorView?.let {
                showSnackBar(it, text, FenixSnackbar.LENGTH_LONG)
            }
        }
    }

    private fun createAddonStyle(context: Context): AddonsManagerAdapter.Style {
        val sectionsTypeFace = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            Typeface.create(Typeface.DEFAULT, FONT_WEIGHT_MEDIUM, false)
        } else {
            Typeface.create(Typeface.DEFAULT, Typeface.BOLD)
        }

        return AddonsManagerAdapter.Style(
            sectionsTextColor = ThemeManager.resolveAttribute(R.attr.textPrimary, context),
            addonNameTextColor = ThemeManager.resolveAttribute(R.attr.textPrimary, context),
            addonSummaryTextColor = ThemeManager.resolveAttribute(R.attr.textSecondary, context),
            sectionsTypeFace = sectionsTypeFace,
            addonAllowPrivateBrowsingLabelDrawableRes = R.drawable.ic_add_on_private_browsing_label,
        )
    }

    @VisibleForTesting
    internal fun provideAddonManger(): AddonManager {
        return requireContext().components.addonManager
    }

    internal fun provideAccessibilityServicesEnabled(): Boolean {
        return requireContext().settings().accessibilityServicesEnabled
    }

    internal fun installAddon(addon: Addon) {
        binding?.addonProgressOverlay?.overlayCardView?.visibility = View.VISIBLE
        if (provideAccessibilityServicesEnabled()) {
            binding?.let { announceForAccessibility(it.addonProgressOverlay.addOnsOverlayText.text) }
        }
        val installOperation = provideAddonManger().installAddon(
            addon,
            onSuccess = {
                runIfFragmentIsAttached {
                    adapter?.updateAddon(it)
                    binding?.addonProgressOverlay?.overlayCardView?.visibility = View.GONE
                }
            },
            onError = { _, _ ->
                binding?.addonProgressOverlay?.overlayCardView?.visibility = View.GONE
            },
        )
        binding?.addonProgressOverlay?.cancelButton?.setOnClickListener {
            lifecycleScope.launch(Dispatchers.Main) {
                val safeBinding = binding
                // Hide the installation progress overlay once cancellation is successful.
                if (installOperation.cancel().await()) {
                    safeBinding?.addonProgressOverlay?.overlayCardView?.visibility = View.GONE
                }
            }
        }
    }

    private fun announceForAccessibility(announcementText: CharSequence) {
        val event = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            AccessibilityEvent(AccessibilityEvent.TYPE_ANNOUNCEMENT)
        } else {
            @Suppress("DEPRECATION")
            AccessibilityEvent.obtain(AccessibilityEvent.TYPE_ANNOUNCEMENT)
        }

        binding?.addonProgressOverlay?.overlayCardView?.onInitializeAccessibilityEvent(event)
        event.text.add(announcementText)
        event.contentDescription = null
        binding?.addonProgressOverlay?.overlayCardView?.let {
            it.parent?.requestSendAccessibilityEvent(
                it,
                event,
            )
        }
    }

    private fun openAMO() {
        (activity as HomeActivity).openToBrowserAndLoad(
            searchTermOrURL = AMO_HOMEPAGE_FOR_ANDROID,
            newTab = true,
            from = BrowserDirection.FromGlobal,
        )
    }

    companion object {
        private const val BUNDLE_KEY_INSTALL_EXTERNAL_ADDON_COMPLETE = "INSTALL_EXTERNAL_ADDON_COMPLETE"

        // This is locale-less on purpose so that the content negotiation happens on the AMO side because the current
        // user language might not be supported by AMO and/or the language might not be exactly what AMO is expecting
        // (e.g. `en` instead of `en-US`).
        private const val AMO_HOMEPAGE_FOR_ANDROID = "https://addons.mozilla.org/android/"
    }
}
