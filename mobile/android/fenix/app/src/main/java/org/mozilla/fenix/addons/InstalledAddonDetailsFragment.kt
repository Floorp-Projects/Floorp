/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.addons

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.core.view.isVisible
import androidx.fragment.app.Fragment
import androidx.lifecycle.lifecycleScope
import androidx.navigation.Navigation
import androidx.navigation.findNavController
import androidx.navigation.fragment.findNavController
import com.google.android.material.switchmaterial.SwitchMaterial
import kotlinx.android.synthetic.main.fragment_installed_add_on_details.view.*
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import mozilla.components.feature.addons.Addon
import mozilla.components.feature.addons.AddonManagerException
import mozilla.components.feature.addons.ui.translateName
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.showToolbar
import org.mozilla.fenix.ext.runIfFragmentIsAttached

/**
 * An activity to show the details of a installed add-on.
 */
@Suppress("LargeClass", "TooManyFunctions")
class InstalledAddonDetailsFragment : Fragment() {
    private lateinit var addon: Addon

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View {
        if (!::addon.isInitialized) {
            addon = AddonDetailsFragmentArgs.fromBundle(requireNotNull(arguments)).addon
        }

        return inflater.inflate(R.layout.fragment_installed_add_on_details, container, false).also {
            bindUI(it)
        }
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        bindAddon(view)
    }

    private fun bindAddon(view: View) {
        lifecycleScope.launch(Dispatchers.IO) {
            try {
                val addons = requireContext().components.addonManager.getAddons()
                lifecycleScope.launch(Dispatchers.Main) {
                    runIfFragmentIsAttached {
                        addons.find { addon.id == it.id }.let {
                            if (it == null) {
                                throw AddonManagerException(Exception("Addon ${addon.id} not found"))
                            } else {
                                addon = it
                                bindUI(view)
                            }
                            view.add_on_progress_bar.isVisible = false
                            view.addon_container.isVisible = true
                        }
                    }
                }
            } catch (e: AddonManagerException) {
                lifecycleScope.launch(Dispatchers.Main) {
                    runIfFragmentIsAttached {
                        showSnackBar(
                            view,
                            getString(R.string.mozac_feature_addons_failed_to_query_add_ons)
                        )
                        findNavController().popBackStack()
                    }
                }
            }
        }
    }

    private fun bindUI(view: View) {
        val title = addon.translateName(view.context)
        showToolbar(title)

        bindEnableSwitch(view)
        bindSettings(view)
        bindDetails(view)
        bindPermissions(view)
        bindAllowInPrivateBrowsingSwitch(view)
        bindRemoveButton(view)
    }

    @SuppressWarnings("LongMethod")
    private fun bindEnableSwitch(view: View) {
        val switch = view.enable_switch
        val privateBrowsingSwitch = view.allow_in_private_browsing_switch
        switch.setState(addon.isEnabled())
        switch.setOnCheckedChangeListener { v, isChecked ->
            val addonManager = v.context.components.addonManager
            switch.isClickable = false
            view.remove_add_on.isEnabled = false
            if (isChecked) {
                addonManager.enableAddon(
                    addon,
                    onSuccess = {
                        runIfFragmentIsAttached {
                            this.addon = it
                            switch.isClickable = true
                            privateBrowsingSwitch.isVisible = it.isEnabled()
                            privateBrowsingSwitch.isChecked = it.isAllowedInPrivateBrowsing()
                            switch.setText(R.string.mozac_feature_addons_enabled)
                            view.settings.isVisible = shouldSettingsBeVisible()
                            view.remove_add_on.isEnabled = true
                            context?.let {
                                showSnackBar(
                                    view,
                                    getString(
                                        R.string.mozac_feature_addons_successfully_enabled,
                                        addon.translateName(it)
                                    )
                                )
                            }
                        }
                    },
                    onError = {
                        runIfFragmentIsAttached {
                            switch.isClickable = true
                            view.remove_add_on.isEnabled = true
                            switch.setState(addon.isEnabled())
                            context?.let {
                                showSnackBar(
                                    view,
                                    getString(
                                        R.string.mozac_feature_addons_failed_to_enable,
                                        addon.translateName(it)
                                    )
                                )
                            }
                        }
                    }
                )
            } else {
                view.settings.isVisible = false
                addonManager.disableAddon(
                    addon,
                    onSuccess = {
                        runIfFragmentIsAttached {
                            this.addon = it
                            switch.isClickable = true
                            privateBrowsingSwitch.isVisible = it.isEnabled()
                            switch.setText(R.string.mozac_feature_addons_disabled)
                            view.remove_add_on.isEnabled = true
                            context?.let {
                                showSnackBar(
                                    view,
                                    getString(
                                        R.string.mozac_feature_addons_successfully_disabled,
                                        addon.translateName(it)
                                    )
                                )
                            }
                        }
                    },
                    onError = {
                        runIfFragmentIsAttached {
                            switch.isClickable = true
                            privateBrowsingSwitch.isClickable = true
                            view.remove_add_on.isEnabled = true
                            switch.setState(addon.isEnabled())
                            context?.let {
                                showSnackBar(
                                    view,
                                    getString(
                                        R.string.mozac_feature_addons_failed_to_disable,
                                        addon.translateName(it)
                                    )
                                )
                            }
                        }
                    }
                )
            }
        }
    }

    private fun bindSettings(view: View) {
        view.settings.apply {
            isVisible = shouldSettingsBeVisible()
            setOnClickListener {
                requireContext().components.analytics.metrics.track(
                    Event.AddonOpenSetting(addon.id)
                )
                val settingUrl = addon.installedState?.optionsPageUrl ?: return@setOnClickListener
                val directions = if (addon.installedState?.openOptionsPageInTab == true) {
                    val components = it.context.components
                    val shouldCreatePrivateSession =
                        (activity as HomeActivity).browsingModeManager.mode.isPrivate

                    components.useCases.tabsUseCases.addTab(settingUrl, private = shouldCreatePrivateSession)

                    InstalledAddonDetailsFragmentDirections.actionGlobalBrowser(null)
                } else {
                    InstalledAddonDetailsFragmentDirections
                        .actionInstalledAddonFragmentToAddonInternalSettingsFragment(addon)
                }
                Navigation.findNavController(this).navigate(directions)
            }
        }
    }

    private fun bindDetails(view: View) {
        view.details.setOnClickListener {
            val directions =
                InstalledAddonDetailsFragmentDirections.actionInstalledAddonFragmentToAddonDetailsFragment(
                    addon
                )
            Navigation.findNavController(view).navigate(directions)
        }
    }

    private fun bindPermissions(view: View) {
        view.permissions.setOnClickListener {
            val directions =
                InstalledAddonDetailsFragmentDirections.actionInstalledAddonFragmentToAddonPermissionsDetailsFragment(
                    addon
                )
            Navigation.findNavController(view).navigate(directions)
        }
    }

    private fun bindAllowInPrivateBrowsingSwitch(view: View) {
        val switch = view.allow_in_private_browsing_switch
        switch.isChecked = addon.isAllowedInPrivateBrowsing()
        switch.isVisible = addon.isEnabled()
        switch.setOnCheckedChangeListener { v, isChecked ->
            val addonManager = v.context.components.addonManager
            switch.isClickable = false
            view.remove_add_on.isEnabled = false
            addonManager.setAddonAllowedInPrivateBrowsing(
                addon,
                isChecked,
                onSuccess = {
                    runIfFragmentIsAttached {
                        this.addon = it
                        switch.isClickable = true
                        view.remove_add_on.isEnabled = true
                    }
                },
                onError = {
                    runIfFragmentIsAttached {
                        switch.isChecked = addon.isAllowedInPrivateBrowsing()
                        switch.isClickable = true
                        view.remove_add_on.isEnabled = true
                    }
                }
            )
        }
    }
    private fun bindRemoveButton(view: View) {
        view.remove_add_on.setOnClickListener {
            setAllInteractiveViewsClickable(view, false)
            requireContext().components.addonManager.uninstallAddon(
                addon,
                onSuccess = {
                    runIfFragmentIsAttached {
                        setAllInteractiveViewsClickable(view, true)
                        context?.let {
                            showSnackBar(
                                view,
                                getString(
                                    R.string.mozac_feature_addons_successfully_uninstalled,
                                    addon.translateName(it)
                                )
                            )
                        }
                        view.findNavController().popBackStack()
                    }
                },
                onError = { _, _ ->
                    runIfFragmentIsAttached {
                        setAllInteractiveViewsClickable(view, true)
                        context?.let {
                            showSnackBar(
                                view,
                                getString(
                                    R.string.mozac_feature_addons_failed_to_uninstall,
                                    addon.translateName(it)
                                )
                            )
                        }
                    }
                }
            )
        }
    }

    private fun setAllInteractiveViewsClickable(view: View, clickable: Boolean) {
        view.enable_switch.isClickable = clickable
        view.settings.isClickable = clickable
        view.details.isClickable = clickable
        view.permissions.isClickable = clickable
        view.remove_add_on.isClickable = clickable
    }

    private fun SwitchMaterial.setState(checked: Boolean) {
        val text = if (checked) {
            R.string.mozac_feature_addons_enabled
        } else {
            R.string.mozac_feature_addons_disabled
        }
        setText(text)
        isChecked = checked
    }

    private fun shouldSettingsBeVisible() = !addon.installedState?.optionsPageUrl.isNullOrEmpty()
}
