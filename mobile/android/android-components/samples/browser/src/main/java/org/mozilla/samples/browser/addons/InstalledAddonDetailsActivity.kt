/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.samples.browser.addons

import android.content.Intent
import android.os.Bundle
import android.view.View
import android.widget.Switch
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import mozilla.components.feature.addons.Addon
import mozilla.components.feature.addons.ui.translate
import mozilla.components.feature.addons.ui.translatedName
import org.mozilla.samples.browser.ext.components
import org.mozilla.samples.browser.BrowserActivity
import org.mozilla.samples.browser.R

/**
 * An activity to show the details of a installed add-on.
 */
class InstalledAddonDetailsActivity : AppCompatActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_installed_add_on_details)
        val addon = requireNotNull(intent.getParcelableExtra<Addon>("add_on"))
        bind(addon)
    }

    private fun bind(addon: Addon) {
        title = addon.translatableName.translate()

        bindEnableSwitch(addon)

        bindSettings(addon)

        bindDetails(addon)

        bindPermissions(addon)

        bindRemoveButton(addon)
    }

    private fun bindEnableSwitch(addon: Addon) {
        val switch = findViewById<Switch>(R.id.enable_switch)
        switch.setState(addon.isEnabled())
        switch.setOnCheckedChangeListener { _, isChecked ->
            if (isChecked) {
                this.components.addonManager.enableAddon(
                    addon,
                    onSuccess = {
                        switch.setState(true)
                        Toast.makeText(
                            this,
                            getString(R.string.mozac_feature_addons_successfully_enabled, addon.translatedName),
                            Toast.LENGTH_SHORT
                        ).show()
                    },
                    onError = {
                        Toast.makeText(
                            this,
                            getString(R.string.mozac_feature_addons_failed_to_enable, addon.translatedName),
                            Toast.LENGTH_SHORT
                        ).show()
                    }
                )
            } else {
                this.components.addonManager.disableAddon(
                    addon,
                    onSuccess = {
                        switch.setState(false)
                        Toast.makeText(
                            this,
                            getString(R.string.mozac_feature_addons_successfully_disabled, addon.translatedName),
                            Toast.LENGTH_SHORT
                        ).show()
                    },
                    onError = {
                        Toast.makeText(
                            this,
                            getString(R.string.mozac_feature_addons_failed_to_disable, addon.translatedName),
                            Toast.LENGTH_SHORT
                        ).show()
                    }
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

    private fun bindRemoveButton(addon: Addon) {
        findViewById<View>(R.id.remove_add_on).setOnClickListener {
            this.components.addonManager.uninstallAddon(
                addon,
                onSuccess = {
                    Toast.makeText(
                        this,
                        getString(R.string.mozac_feature_addons_successfully_uninstalled, addon.translatedName),
                        Toast.LENGTH_SHORT
                    ).show()
                    finish()
                },
                onError = { _, _ ->
                    Toast.makeText(
                        this,
                        getString(R.string.mozac_feature_addons_failed_to_uninstall, addon.translatedName),
                        Toast.LENGTH_SHORT
                    ).show()
                }
            )
        }
    }

    private fun Switch.setState(checked: Boolean) {
        val text = if (checked) {
            R.string.mozac_feature_addons_enabled
        } else {
            R.string.mozac_feature_addons_disabled
        }
        setText(text)
        isChecked = checked
    }
}
