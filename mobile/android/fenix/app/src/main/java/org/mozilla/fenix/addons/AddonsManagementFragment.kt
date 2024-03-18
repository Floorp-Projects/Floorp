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
import android.view.accessibility.AccessibilityNodeInfo
import androidx.annotation.VisibleForTesting
import androidx.core.view.isVisible
import androidx.fragment.app.Fragment
import androidx.lifecycle.lifecycleScope
import androidx.navigation.fragment.findNavController
import androidx.recyclerview.widget.LinearLayoutManager
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Dispatchers.IO
import kotlinx.coroutines.launch
import mozilla.components.concept.engine.webextension.InstallationMethod
import mozilla.components.feature.addons.Addon
import mozilla.components.feature.addons.AddonManager
import mozilla.components.feature.addons.AddonManagerException
import mozilla.components.feature.addons.ui.AddonsManagerAdapter
import mozilla.components.feature.addons.ui.AddonsManagerAdapterDelegate
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
import org.mozilla.fenix.settings.SupportUtils
import org.mozilla.fenix.theme.ThemeManager

/**
 * Fragment use for managing add-ons.
 */
@Suppress("TooManyFunctions", "LargeClass")
class AddonsManagementFragment : Fragment(R.layout.fragment_add_ons_management) {

    private var binding: FragmentAddOnsManagementBinding? = null

    private var addons: List<Addon> = emptyList()

    private var adapter: AddonsManagerAdapter? = null

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        binding = FragmentAddOnsManagementBinding.bind(view)
        bindRecyclerView()
        (activity as HomeActivity).webExtensionPromptFeature.onAddonChanged = {
            runIfFragmentIsAttached {
                adapter?.updateAddon(it)
            }
        }
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
        (activity as HomeActivity).webExtensionPromptFeature.onAddonChanged = {}
    }

    private fun bindRecyclerView() {
        val managementView = AddonsManagementView(
            navController = findNavController(),
            onInstallButtonClicked = ::installAddon,
            onMoreAddonsButtonClicked = ::openAMO,
            onLearnMoreClicked = ::openLearnMoreLink,
        )

        val recyclerView = binding?.addOnsList
        recyclerView?.layoutManager = LinearLayoutManager(requireContext())
        val shouldRefresh = adapter != null

        lifecycleScope.launch(IO) {
            try {
                addons = requireContext().components.addonManager.getAddons()
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
                                addonsManagerDelegate = managementView,
                                addons = addons,
                                style = createAddonStyle(requireContext()),
                                excludedAddonIDs = excludedAddonIDs,
                                store = requireComponents.core.store,
                            )
                        }
                        binding?.addOnsProgressBar?.isVisible = false
                        binding?.addOnsEmptyMessage?.isVisible = false

                        recyclerView?.adapter = adapter
                        recyclerView?.accessibilityDelegate = object : View.AccessibilityDelegate() {
                            override fun onInitializeAccessibilityNodeInfo(host: View, info: AccessibilityNodeInfo) {
                                super.onInitializeAccessibilityNodeInfo(host, info)

                                adapter?.let {
                                    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
                                        info.collectionInfo = AccessibilityNodeInfo.CollectionInfo(
                                            it.itemCount,
                                            1,
                                            false,
                                        )
                                    } else {
                                        @Suppress("DEPRECATION")
                                        info.collectionInfo = AccessibilityNodeInfo.CollectionInfo.obtain(
                                            it.itemCount,
                                            1,
                                            false,
                                        )
                                    }
                                }
                            }
                        }

                        if (shouldRefresh) {
                            adapter?.updateAddons(addons)
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
            url = addon.downloadUrl,
            installationMethod = InstallationMethod.MANAGER,
            onSuccess = {
                runIfFragmentIsAttached {
                    adapter?.updateAddon(it)
                    binding?.addonProgressOverlay?.overlayCardView?.visibility = View.GONE
                }
            },
            onError = { _ ->
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
        openLinkInNewTab(AMO_HOMEPAGE_FOR_ANDROID)
    }

    private fun openLearnMoreLink(link: AddonsManagerAdapterDelegate.LearnMoreLinks, addon: Addon) {
        val url = when (link) {
            AddonsManagerAdapterDelegate.LearnMoreLinks.BLOCKLISTED_ADDON ->
                "${BuildConfig.AMO_BASE_URL}/android/blocked-addon/${addon.id}/"
            AddonsManagerAdapterDelegate.LearnMoreLinks.ADDON_NOT_CORRECTLY_SIGNED ->
                SupportUtils.getSumoURLForTopic(requireContext(), SupportUtils.SumoTopic.UNSIGNED_ADDONS)
        }
        openLinkInNewTab(url)
    }

    private fun openLinkInNewTab(url: String) {
        (activity as HomeActivity).openToBrowserAndLoad(
            searchTermOrURL = url,
            newTab = true,
            from = BrowserDirection.FromAddonsManagementFragment,
        )
    }

    companion object {
        // This is locale-less on purpose so that the content negotiation happens on the AMO side because the current
        // user language might not be supported by AMO and/or the language might not be exactly what AMO is expecting
        // (e.g. `en` instead of `en-US`).
        private const val AMO_HOMEPAGE_FOR_ANDROID = "${BuildConfig.AMO_BASE_URL}/android/"
    }
}
