/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.quicksettings

import android.content.Context
import androidx.annotation.DrawableRes
import androidx.annotation.StringRes
import mozilla.components.concept.engine.permission.SitePermissions
import mozilla.components.concept.engine.permission.SitePermissions.AutoplayStatus
import mozilla.components.feature.sitepermissions.SitePermissionsRules
import mozilla.components.feature.sitepermissions.SitePermissionsRules.AutoplayAction
import mozilla.components.lib.state.State
import org.mozilla.fenix.R
import org.mozilla.fenix.settings.PhoneFeature
import org.mozilla.fenix.trackingprotection.ProtectionsState
import org.mozilla.fenix.utils.Settings

/**
 * [State] containing all data displayed to the user by this Fragment.
 *
 * Partitioned further to contain multiple states for each standalone View this Fragment holds.
 */
data class QuickSettingsFragmentState(
    val webInfoState: WebsiteInfoState,
    val websitePermissionsState: WebsitePermissionsState,
    val protectionsState: ProtectionsState,
) : State

/**
 * [State] to be rendered by [WebsiteInfoView] indicating whether the connection is secure or not.
 *
 * @param websiteUrl [String] the URL of the current web page.
 * @param websiteTitle [String] the title of the current web page.
 * @param websiteSecurityUiValues UI values to represent the security of the website.
 */
data class WebsiteInfoState(
    val websiteUrl: String,
    val websiteTitle: String,
    val websiteSecurityUiValues: WebsiteSecurityUiValues,
    val certificateName: String,
) : State {
    companion object {
        /**
         * Construct an initial [WebsiteInfoState]
         * based on the current website's status and connection.
         * While being displayed users have no way of modifying it.
         *
         * @param websiteUrl [String] the URL of the current web page.
         * @param websiteTitle [String] the title of the current web page.
         * @param isSecured [Boolean] whether the connection is secured (TLS) or not.
         * @param certificateName [String] the certificate name of the current web  page.
         */
        fun createWebsiteInfoState(
            websiteUrl: String,
            websiteTitle: String,
            isSecured: Boolean,
            certificateName: String,
        ): WebsiteInfoState {
            val uiValues =
                if (isSecured) WebsiteSecurityUiValues.SECURE else WebsiteSecurityUiValues.INSECURE
            return WebsiteInfoState(websiteUrl, websiteTitle, uiValues, certificateName)
        }
    }
}

enum class WebsiteSecurityUiValues(
    @StringRes val securityInfoRes: Int,
    @DrawableRes val iconRes: Int,
) {
    SECURE(
        R.string.quick_settings_sheet_secure_connection_2,
        R.drawable.ic_lock,
    ),
    INSECURE(
        R.string.quick_settings_sheet_insecure_connection_2,
        R.drawable.mozac_ic_broken_lock,
    ),
}

/**
 * [State] to be rendered by [WebsitePermissionsView] displaying all explicitly allowed or blocked
 * website permissions.
 */
typealias WebsitePermissionsState = Map<PhoneFeature, WebsitePermission>

/**
 * Wrapper over a website permission encompassing all it's needed state to be rendered on the screen.
 *
 * Contains a limited number of implementations because there is a known, finite number of permissions
 * we need to display to the user.
 *
 * @property status The *allowed* / *blocked* permission status to be shown to the user.
 * @property isVisible Whether this permission should be shown to the user.
 * @property isEnabled Visual indication about whether this permission is *enabled* / *disabled*
 * @property isBlockedByAndroid Whether the corresponding *dangerous* Android permission is granted
 * for the app by the user or not.
 */
sealed class WebsitePermission(
    open val phoneFeature: PhoneFeature,
    open val status: String,
    open val isVisible: Boolean,
    open val isEnabled: Boolean,
    open val isBlockedByAndroid: Boolean,
) {
    data class Autoplay(
        val autoplayValue: AutoplayValue,
        val options: List<AutoplayValue>,
        override val isVisible: Boolean,
    ) : WebsitePermission(
        PhoneFeature.AUTOPLAY,
        autoplayValue.label,
        isVisible,
        autoplayValue.isEnabled,
        isBlockedByAndroid = false,
    )

    data class Toggleable(
        override val phoneFeature: PhoneFeature,
        override val status: String,
        override val isVisible: Boolean,
        override val isEnabled: Boolean,
        override val isBlockedByAndroid: Boolean,
    ) : WebsitePermission(
        phoneFeature,
        status,
        isVisible,
        isEnabled,
        isBlockedByAndroid,
    )
}

sealed class AutoplayValue(
    open val label: String,
    open val rules: SitePermissionsRules,
    open val sitePermission: SitePermissions?,
) {
    override fun toString() = label
    abstract fun isSelected(): Boolean
    abstract fun createSitePermissionsFromCustomRules(origin: String, settings: Settings): SitePermissions
    abstract fun updateSitePermissions(sitePermissions: SitePermissions): SitePermissions
    abstract val isEnabled: Boolean

    val isVisible: Boolean get() = isSelected()

    data class AllowAll(
        override val label: String,
        override val rules: SitePermissionsRules,
        override val sitePermission: SitePermissions?,
    ) : AutoplayValue(label, rules, sitePermission) {
        override val isEnabled: Boolean = true
        override fun toString() = super.toString()
        override fun isSelected(): Boolean {
            val actions = if (sitePermission !== null) {
                listOf(
                    sitePermission.autoplayAudible,
                    sitePermission.autoplayInaudible,
                )
            } else {
                listOf(rules.autoplayAudible.toAutoplayStatus(), rules.autoplayInaudible.toAutoplayStatus())
            }

            return actions.all { it == AutoplayStatus.ALLOWED }
        }

        override fun createSitePermissionsFromCustomRules(origin: String, settings: Settings): SitePermissions {
            val rules = settings.getSitePermissionsCustomSettingsRules()
            return rules.copy(
                autoplayAudible = AutoplayAction.ALLOWED,
                autoplayInaudible = AutoplayAction.ALLOWED,
            ).toSitePermissions(origin)
        }

        override fun updateSitePermissions(sitePermissions: SitePermissions): SitePermissions {
            return sitePermissions.copy(
                autoplayAudible = AutoplayStatus.ALLOWED,
                autoplayInaudible = AutoplayStatus.ALLOWED,
            )
        }
    }

    data class BlockAll(
        override val label: String,
        override val rules: SitePermissionsRules,
        override val sitePermission: SitePermissions?,
    ) : AutoplayValue(label, rules, sitePermission) {
        override val isEnabled: Boolean = false
        override fun toString() = super.toString()
        override fun isSelected(): Boolean {
            val actions = if (sitePermission !== null) {
                listOf(
                    sitePermission.autoplayAudible,
                    sitePermission.autoplayInaudible,
                )
            } else {
                listOf(rules.autoplayAudible.toAutoplayStatus(), rules.autoplayInaudible.toAutoplayStatus())
            }

            return actions.all { it == AutoplayStatus.BLOCKED }
        }
        override fun createSitePermissionsFromCustomRules(origin: String, settings: Settings): SitePermissions {
            val rules = settings.getSitePermissionsCustomSettingsRules()
            return rules.copy(
                autoplayAudible = AutoplayAction.BLOCKED,
                autoplayInaudible = AutoplayAction.BLOCKED,
            ).toSitePermissions(origin)
        }

        override fun updateSitePermissions(sitePermissions: SitePermissions): SitePermissions {
            return sitePermissions.copy(
                autoplayAudible = AutoplayStatus.BLOCKED,
                autoplayInaudible = AutoplayStatus.BLOCKED,
            )
        }
    }

    data class BlockAudible(
        override val label: String,
        override val rules: SitePermissionsRules,
        override val sitePermission: SitePermissions?,
    ) : AutoplayValue(label, rules, sitePermission) {
        override val isEnabled: Boolean = false
        override fun toString() = super.toString()
        override fun isSelected(): Boolean {
            val (audible, inaudible) = if (sitePermission !== null) {
                sitePermission.autoplayAudible to sitePermission.autoplayInaudible
            } else {
                rules.autoplayAudible.toAutoplayStatus() to rules.autoplayInaudible.toAutoplayStatus()
            }

            return audible == AutoplayStatus.BLOCKED && inaudible == AutoplayStatus.ALLOWED
        }

        override fun createSitePermissionsFromCustomRules(origin: String, settings: Settings): SitePermissions {
            val rules = settings.getSitePermissionsCustomSettingsRules()
            return rules.copy(autoplayAudible = AutoplayAction.BLOCKED, autoplayInaudible = AutoplayAction.ALLOWED)
                .toSitePermissions(origin)
        }

        override fun updateSitePermissions(sitePermissions: SitePermissions): SitePermissions {
            return sitePermissions.copy(
                autoplayInaudible = AutoplayStatus.ALLOWED,
                autoplayAudible = AutoplayStatus.BLOCKED,
            )
        }
    }

    companion object {
        fun values(
            context: Context,
            settings: Settings,
            sitePermission: SitePermissions?,
        ): List<AutoplayValue> {
            val rules = settings.getSitePermissionsCustomSettingsRules()
            return listOf(
                AllowAll(
                    context.getString(R.string.quick_setting_option_autoplay_allowed),
                    rules,
                    sitePermission,
                ),
                BlockAll(
                    context.getString(R.string.quick_setting_option_autoplay_blocked),
                    rules,
                    sitePermission,
                ),
                BlockAudible(
                    context.getString(R.string.quick_setting_option_autoplay_block_audio),
                    rules,
                    sitePermission,
                ),
            )
        }

        fun getFallbackValue(
            context: Context,
            settings: Settings,
            sitePermission: SitePermissions?,
        ): AutoplayValue {
            val rules = settings.getSitePermissionsCustomSettingsRules()
            return BlockAudible(
                context.getString(R.string.preference_option_autoplay_block_audio2),
                rules,
                sitePermission,
            )
        }
    }
}
