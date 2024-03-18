/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.widget

import android.app.role.RoleManager
import android.content.Context
import android.os.Build
import android.util.AttributeSet
import androidx.preference.Preference
import androidx.preference.PreferenceViewHolder
import com.google.android.material.switchmaterial.SwitchMaterial
import mozilla.components.support.utils.Browsers
import mozilla.components.support.utils.ext.navigateToDefaultBrowserAppsSettings
import org.mozilla.focus.GleanMetrics.SetDefaultBrowser
import org.mozilla.focus.R
import org.mozilla.focus.ext.tryAsActivity
import org.mozilla.focus.utils.SupportUtils.openDefaultBrowserSumoPage

class DefaultBrowserPreference @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = 0,
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
                            RoleManager.ROLE_BROWSER,
                        )
                    ) {
                        context.tryAsActivity()?.startActivityForResult(
                            it.createRequestRoleIntent(RoleManager.ROLE_BROWSER),
                            REQUEST_CODE_BROWSER_ROLE,
                        )
                        SetDefaultBrowser.fromAppSettings.record(
                            SetDefaultBrowser.FromAppSettingsExtra(
                                isDefault,
                            ),
                        )
                    } else {
                        context.navigateToDefaultBrowserAppsSettings()
                        SetDefaultBrowser.fromOsSettings.record(SetDefaultBrowser.FromOsSettingsExtra(isDefault))
                    }
                }
            }
            Build.VERSION.SDK_INT >= Build.VERSION_CODES.N -> {
                context.navigateToDefaultBrowserAppsSettings()
                SetDefaultBrowser.fromOsSettings.record(SetDefaultBrowser.FromOsSettingsExtra(isDefault))
            }
            else -> {
                openDefaultBrowserSumoPage(context)
                SetDefaultBrowser.learnMoreOpened.record(
                    SetDefaultBrowser.LearnMoreOpenedExtra(
                        isDefault,
                    ),
                )
            }
        }
    }

    companion object {
        const val REQUEST_CODE_BROWSER_ROLE = 1
    }
}
