/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.samples.browser.addons

import android.content.Intent
import android.os.Bundle
import android.view.View
import android.widget.Switch
import android.widget.TextView
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import mozilla.components.feature.addons.Addon
import org.mozilla.samples.browser.R
import org.mozilla.samples.browser.ext.components

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

    private fun bindVersion(addon: Addon) {
        val versionView = findViewById<TextView>(R.id.version_text)
        versionView.text = addon.version
    }

    private fun bindEnableSwitch(addon: Addon) {
        val switch = findViewById<Switch>(R.id.enable_switch)
        switch.setState(addon.isEnabled())
        switch.setOnCheckedChangeListener { _, isChecked ->
            if (isChecked) {
                this.components.addonManager.disableAddon(
                    addon,
                    onSuccess = {
                        Toast.makeText(
                            this,
                            "Successfully disabled ${addon.translatableName.translate()}",
                            Toast.LENGTH_SHORT
                        ).show()
                    },
                    onError = {
                        Toast.makeText(
                            this,
                            "Failed to disable ${addon.translatableName.translate()}",
                            Toast.LENGTH_SHORT
                        ).show()
                    }
                )
            } else {
                this.components.addonManager.enableAddon(
                    addon,
                    onSuccess = {
                        Toast.makeText(
                            this,
                            "Successfully enabled ${addon.translatableName.translate()}",
                            Toast.LENGTH_SHORT
                        ).show()
                    },
                    onError = {
                        Toast.makeText(
                            this,
                            "Failed to enable ${addon.translatableName.translate()}",
                            Toast.LENGTH_SHORT
                        ).show()
                    }
                )
            }
        }
    }

    private fun bindSettings(addOn: Addon) {
        val view = findViewById<View>(R.id.settings)
        view.isEnabled = addOn.installedState?.optionsPageUrl != null
        view.setOnClickListener {
            val intent = Intent(this, AddonSettingsActivity::class.java)
            intent.putExtra("add_on", addOn)
            this.startActivity(intent)
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
                        "Successfully uninstalled ${addon.translatableName.translate()}",
                        Toast.LENGTH_SHORT
                    ).show()
                    finish()
                },
                onError = { _, _ ->
                    Toast.makeText(
                        this,
                        "Failed to uninstall ${addon.translatableName.translate()}",
                        Toast.LENGTH_SHORT
                    ).show()
                }
            )
        }
    }

    private fun Switch.setState(checked: Boolean) {
        val text = if (checked) {
            R.string.addon_settings_on
        } else {
            R.string.addon_settings_off
        }
        setText(text)
        isChecked = checked
    }
}
