/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.samples.browser.addons

import android.content.Intent
import android.os.Bundle
import android.view.View
import android.widget.Toast
import androidx.annotation.StringRes
import androidx.appcompat.app.AppCompatActivity
import androidx.appcompat.widget.SwitchCompat
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import mozilla.components.feature.addons.Addon
import mozilla.components.feature.addons.AddonManagerException
import mozilla.components.feature.addons.ui.translateName
import mozilla.components.support.utils.ext.getParcelableExtraCompat
import org.mozilla.samples.browser.BrowserActivity
import org.mozilla.samples.browser.R
import org.mozilla.samples.browser.ext.components
import mozilla.components.feature.addons.R as addonsR

/**
 * An activity to show the details of a installed add-on.
 */
@Suppress("LargeClass")
class InstalledAddonDetailsActivity : AppCompatActivity() {
    private val scope = CoroutineScope(Dispatchers.IO)

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_installed_add_on_details)
        val addon = requireNotNull(intent.getParcelableExtraCompat("add_on", Addon::class.java))
        bindAddon(addon)
    }

    private fun bindAddon(addon: Addon) {
        scope.launch {
            try {
                val context = baseContext
                val addons = context.components.addonManager.getAddons()
                scope.launch(Dispatchers.Main) {
                    addons.find { addon.id == it.id }.let {
                        if (it == null) {
                            throw AddonManagerException(Exception("Addon ${addon.id} not found"))
                        } else {
                            bindUI(it)
                        }
                    }
                }
            } catch (e: AddonManagerException) {
                scope.launch(Dispatchers.Main) {
                    Toast.makeText(
                        baseContext,
                        addonsR.string.mozac_feature_addons_failed_to_query_add_ons,
                        Toast.LENGTH_SHORT,
                    ).show()
                }
            }
        }
    }

    private fun bindUI(addon: Addon) {
        title = addon.translateName(this)

        bindEnableSwitch(addon)

        bindSettings(addon)

        bindDetails(addon)

        bindPermissions(addon)

        bindAllowInPrivateBrowsingSwitch(addon)

        bindRemoveButton(addon)
    }

    private fun bindEnableSwitch(addon: Addon) {
        val switch = findViewById<SwitchCompat>(R.id.enable_switch)
        switch.setState(addon.isEnabled())
        switch.setOnCheckedChangeListener { _, isChecked ->
            if (isChecked) {
                this.components.addonManager.enableAddon(
                    addon,
                    onSuccess = {
                        switch.setState(true)
                        showAddonToast(
                            addonsR.string.mozac_feature_addons_successfully_enabled,
                            addon,
                        )
                    },
                    onError = {
                        showAddonToast(
                            addonsR.string.mozac_feature_addons_failed_to_enable,
                            addon,
                        )
                    },
                )
            } else {
                this.components.addonManager.disableAddon(
                    addon,
                    onSuccess = {
                        switch.setState(false)
                        showAddonToast(
                            addonsR.string.mozac_feature_addons_successfully_disabled,
                            addon,
                        )
                    },
                    onError = {
                        showAddonToast(
                            addonsR.string.mozac_feature_addons_failed_to_disable,
                            addon,
                        )
                    },
                )
            }
        }
    }

    private fun bindSettings(addon: Addon) {
        val view = findViewById<View>(R.id.settings)
        val optionsPageUrl = addon.installedState?.optionsPageUrl
        view.isEnabled = optionsPageUrl != null
        view.setOnClickListener {
            if (addon.installedState?.openOptionsPageInTab == true) {
                components.tabsUseCases.addTab(optionsPageUrl as String)
                val intent = Intent(this, BrowserActivity::class.java)
                intent.addFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP)
                this.startActivity(intent)
            } else {
                val intent = Intent(this, AddonSettingsActivity::class.java)
                intent.putExtra("add_on", addon)
                this.startActivity(intent)
            }
        }
    }

    private fun bindDetails(addon: Addon) {
        findViewById<View>(R.id.details).setOnClickListener {
            val intent = Intent(this, AddonDetailsActivity::class.java)
            intent.putExtra("add_on", addon)
            this.startActivity(intent)
        }
    }

    private fun bindPermissions(addon: Addon) {
        findViewById<View>(R.id.permissions).setOnClickListener {
            val intent = Intent(this, PermissionsDetailsActivity::class.java)
            intent.putExtra("add_on", addon)
            this.startActivity(intent)
        }
    }

    private fun bindAllowInPrivateBrowsingSwitch(addon: Addon) {
        val switch = findViewById<SwitchCompat>(R.id.allow_in_private_browsing_switch)
        switch.isChecked = addon.isAllowedInPrivateBrowsing()
        switch.setOnCheckedChangeListener { _, isChecked ->
            this.components.addonManager.setAddonAllowedInPrivateBrowsing(
                addon,
                isChecked,
                onSuccess = {
                    switch.isChecked = isChecked
                },
            )
        }
    }

    private fun bindRemoveButton(addon: Addon) {
        findViewById<View>(R.id.remove_add_on).setOnClickListener {
            this.components.addonManager.uninstallAddon(
                addon,
                onSuccess = {
                    showAddonToast(
                        addonsR.string.mozac_feature_addons_successfully_uninstalled,
                        addon,
                    )
                    finish()
                },
                onError = { _, _ ->
                    showAddonToast(
                        addonsR.string.mozac_feature_addons_failed_to_uninstall,
                        addon,
                    )
                },
            )
        }
    }

    private fun showAddonToast(@StringRes textId: Int, addon: Addon) {
        Toast.makeText(
            this,
            getString(textId, addon.translateName(context = this)),
            Toast.LENGTH_SHORT,
        ).show()
    }

    private fun SwitchCompat.setState(checked: Boolean) {
        val text = if (checked) {
            addonsR.string.mozac_feature_addons_enabled
        } else {
            addonsR.string.mozac_feature_addons_disabled
        }
        setText(text)
        isChecked = checked
    }
}
