/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.addons

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.Switch
import androidx.core.view.isVisible
import androidx.fragment.app.Fragment
import androidx.navigation.Navigation
import androidx.navigation.findNavController
import kotlinx.android.synthetic.main.fragment_installed_add_on_details.view.*
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.cancel
import kotlinx.coroutines.flow.collect
import kotlinx.coroutines.flow.map
import mozilla.components.feature.addons.Addon
import mozilla.components.feature.addons.ui.translate
import mozilla.components.feature.addons.ui.translatedName
import mozilla.components.lib.state.ext.flowScoped
import mozilla.components.support.ktx.kotlinx.coroutines.flow.ifChanged
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.showToolbar

/**
 * An activity to show the details of a installed add-on.
 */
@Suppress("LargeClass", "TooManyFunctions")
class InstalledAddonDetailsFragment : Fragment() {
    private lateinit var addon: Addon
    private var scope: CoroutineScope? = null

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View {
        if (!::addon.isInitialized) {
            addon = AddonDetailsFragmentArgs.fromBundle(requireNotNull(arguments)).addon
        }

        return inflater.inflate(R.layout.fragment_installed_add_on_details, container, false).also {
            bind(it)
        }
    }

    @UseExperimental(ExperimentalCoroutinesApi::class)
    override fun onStart() {
        super.onStart()
        scope = requireContext().components.core.store.flowScoped { flow ->
            flow.ifChanged { it.extensions }
                .map { it.extensions.filterValues { extension -> extension.id == addon.id } }
                .ifChanged()
                .collect {
                    val addonState = it[addon.id]
                    if (addonState != null && addonState.enabled != addon.isEnabled()) {
                        view?.let { view ->
                            val newState = addon.installedState?.copy(enabled = addonState.enabled)
                            this.addon = addon.copy(installedState = newState)
                            view.enable_switch.setState(addon.isEnabled())
                        }
                    }
                }
        }
    }

    override fun onStop() {
        super.onStop()
        scope?.cancel()
    }

    private fun bind(view: View) {
        val title = addon.translatableName.translate()
        showToolbar(title)

        bindEnableSwitch(view)
        bindSettings(view)
        bindDetails(view)
        bindPermissions(view)
        bindRemoveButton(view)
    }

    @SuppressWarnings("LongMethod")
    private fun bindEnableSwitch(view: View) {
        val switch = view.enable_switch
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
                            switch.isClickable = true
                            switch.setText(R.string.mozac_feature_addons_settings_on)
                            view.settings.isVisible = true
                            view.remove_add_on.isEnabled = true
                            this.addon = it
                            showSnackBar(
                                view,
                                getString(
                                    R.string.mozac_feature_addons_successfully_enabled,
                                    addon.translatedName
                                )
                            )
                        }
                    },
                    onError = {
                        runIfFragmentIsAttached {
                            switch.isClickable = true
                            view.remove_add_on.isEnabled = true
                            showSnackBar(
                                view,
                                getString(
                                    R.string.mozac_feature_addons_failed_to_enable,
                                    addon.translatedName
                                )
                            )
                        }
                    }
                )
            } else {
                view.settings.isVisible = false
                addonManager.disableAddon(
                    addon,
                    onSuccess = {
                        runIfFragmentIsAttached {
                            switch.isClickable = true
                            switch.setText(R.string.mozac_feature_addons_settings_off)
                            view.remove_add_on.isEnabled = true
                            this.addon = it
                            showSnackBar(
                                view,
                                getString(
                                    R.string.mozac_feature_addons_successfully_disabled,
                                    addon.translatedName
                                )
                            )
                        }
                    },
                    onError = {
                        runIfFragmentIsAttached {
                            switch.isClickable = true
                            view.remove_add_on.isEnabled = true
                            showSnackBar(
                                view,
                                getString(
                                    R.string.mozac_feature_addons_failed_to_disable,
                                    addon.translatedName
                                )
                            )
                        }
                    }
                )
            }
        }
    }

    private fun bindSettings(view: View) {
        view.settings.apply {
            isVisible = !addon.installedState?.optionsPageUrl.isNullOrEmpty()
            setOnClickListener {
                val directions =
                    InstalledAddonDetailsFragmentDirections.actionInstalledAddonFragmentToAddonInternalSettingsFragment(
                        addon
                    )
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

    private fun bindRemoveButton(view: View) {
        view.remove_add_on.setOnClickListener {
            setAllInteractiveViewsClickable(view, false)
            requireContext().components.addonManager.uninstallAddon(
                addon,
                onSuccess = {
                    runIfFragmentIsAttached {
                        setAllInteractiveViewsClickable(view, true)
                        showSnackBar(
                            view,
                            getString(
                                R.string.mozac_feature_addons_successfully_uninstalled,
                                addon.translatedName
                            )
                        )
                        view.findNavController().popBackStack()
                    }
                },
                onError = { _, _ ->
                    runIfFragmentIsAttached {
                        setAllInteractiveViewsClickable(view, true)
                        showSnackBar(
                            view,
                            getString(
                                R.string.mozac_feature_addons_failed_to_uninstall,
                                addon.translatedName
                            )
                        )
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

    private fun Switch.setState(checked: Boolean) {
        val text = if (checked) {
            R.string.mozac_feature_addons_settings_on
        } else {
            R.string.mozac_feature_addons_settings_off
        }
        setText(text)
        isChecked = checked
    }
}
