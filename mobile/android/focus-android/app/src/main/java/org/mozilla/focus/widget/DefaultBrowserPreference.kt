/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.widget

import android.app.role.RoleManager
import android.content.Context
import android.content.Intent
import android.os.Build
import android.provider.Settings
import android.util.AttributeSet
import androidx.core.os.bundleOf
import androidx.preference.Preference
import androidx.preference.PreferenceViewHolder
import com.google.android.material.switchmaterial.SwitchMaterial
import mozilla.components.support.utils.Browsers
import org.mozilla.focus.GleanMetrics.SetDefaultBrowser
import org.mozilla.focus.R
import org.mozilla.focus.ext.tryAsActivity
import org.mozilla.focus.utils.SupportUtils.openDefaultBrowserSumoPage

class DefaultBrowserPreference @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = 0
) : Preference(context, attrs, defStyleAttr) {

    private var switchView: SwitchMaterial? = null
    private val browsers
        get() = Browsers.all(context)

    init {
        widgetLayoutResource = R.layout.preference_default_browser
        val appName = context.resources.getString(R.string.app_name)
        val title = context.resources.getString(R.string.preference_default_browser2, appName)
        setTitle(title)
    }

    override fun onBindViewHolder(holder: PreferenceViewHolder) {
        super.onBindViewHolder(holder)
        switchView = holder.findViewById(R.id.switch_widget) as SwitchMaterial
        update()
    }

    fun update() {
        switchView?.isChecked = browsers.isDefaultBrowser
    }

    public override fun onClick() {
        val isDefault = browsers.isDefaultBrowser
        when {
            Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q -> {
                context.getSystemService(RoleManager::class.java).also {
                    if (it.isRoleAvailable(RoleManager.ROLE_BROWSER) && !it.isRoleHeld(
                            RoleManager.ROLE_BROWSER
                        )
                    ) {
                        context.tryAsActivity()?.startActivityForResult(
                            it.createRequestRoleIntent(RoleManager.ROLE_BROWSER),
                            REQUEST_CODE_BROWSER_ROLE
                        )
                        SetDefaultBrowser.fromAppSettings.record(
                            SetDefaultBrowser.FromAppSettingsExtra(
                                isDefault
                            )
                        )
                    } else {
                        navigateToDefaultBrowserAppsSettings(isDefault)
                    }
                }
            }
            Build.VERSION.SDK_INT >= Build.VERSION_CODES.N -> {
                navigateToDefaultBrowserAppsSettings(isDefault)
            }
            else -> {
                openDefaultBrowserSumoPage(context)
                SetDefaultBrowser.learnMoreOpened.record(
                    SetDefaultBrowser.LearnMoreOpenedExtra(
                        isDefault
                    )
                )
            }
        }
    }

    private fun navigateToDefaultBrowserAppsSettings(isDefault: Boolean) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
            val intent = Intent(Settings.ACTION_MANAGE_DEFAULT_APPS_SETTINGS)
            intent.putExtra(
                SETTINGS_SELECT_OPTION_KEY,
                DEFAULT_BROWSER_APP_OPTION
            )
            intent.putExtra(
                SETTINGS_SHOW_FRAGMENT_ARGS,
                bundleOf(SETTINGS_SELECT_OPTION_KEY to DEFAULT_BROWSER_APP_OPTION)
            )

            context.startActivity(intent)
            SetDefaultBrowser.fromOsSettings.record(SetDefaultBrowser.FromOsSettingsExtra(isDefault))
        }
    }

    companion object {
        const val REQUEST_CODE_BROWSER_ROLE = 1
        const val SETTINGS_SELECT_OPTION_KEY = ":settings:fragment_args_key"
        const val SETTINGS_SHOW_FRAGMENT_ARGS = ":settings:show_fragment_args"
        const val DEFAULT_BROWSER_APP_OPTION = "default_browser"
    }
}
