/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.quicksettings

import android.graphics.drawable.Drawable
import android.view.View
import android.view.View.GONE
import android.view.View.VISIBLE
import android.view.ViewGroup
import android.widget.TextView
import androidx.appcompat.content.res.AppCompatResources
import androidx.core.content.ContextCompat
import androidx.core.net.toUri
import io.reactivex.Observable
import io.reactivex.Observer
import io.reactivex.functions.Consumer
import kotlinx.android.synthetic.main.fragment_quick_settings_dialog_sheet.*
import mozilla.components.feature.sitepermissions.SitePermissions
import mozilla.components.feature.sitepermissions.SitePermissions.Status.BLOCKED
import mozilla.components.feature.sitepermissions.SitePermissions.Status.NO_DECISION
import mozilla.components.support.ktx.android.net.hostWithoutCommonPrefixes
import org.mozilla.fenix.R
import org.mozilla.fenix.mvi.UIView
import org.mozilla.fenix.settings.PhoneFeature
import org.mozilla.fenix.settings.PhoneFeature.CAMERA
import org.mozilla.fenix.settings.PhoneFeature.LOCATION
import org.mozilla.fenix.settings.PhoneFeature.MICROPHONE
import org.mozilla.fenix.settings.PhoneFeature.NOTIFICATION
import org.mozilla.fenix.utils.Settings

@Suppress("TooManyFunctions")
class QuickSettingsUIView(
    container: ViewGroup,
    actionEmitter: Observer<QuickSettingsAction>,
    changesObservable: Observable<QuickSettingsChange>,
    override val view: View
) : UIView<QuickSettingsState, QuickSettingsAction, QuickSettingsChange>(
    container, actionEmitter, changesObservable
) {
    private val blockedByAndroidPhoneFeatures = mutableListOf<PhoneFeature>()
    private val context get() = view.context
    private val settings: Settings = Settings.getInstance(context)

    override fun updateView() = Consumer<QuickSettingsState> { state ->
        when (state.mode) {
            is QuickSettingsState.Mode.Normal -> {
                bindUrl(state.mode.url)
                bindSecurityInfo(state.mode.isSecured)
                bindReportSiteIssueAction(state.mode.url)
                bindTrackingProtectionAction()
                bindTrackingProtectionInfo(state.mode.isTrackingProtectionOn)
                bindPhoneFeatureItem(CAMERA, state.mode.sitePermissions)
                bindPhoneFeatureItem(MICROPHONE, state.mode.sitePermissions)
                bindPhoneFeatureItem(NOTIFICATION, state.mode.sitePermissions)
                bindPhoneFeatureItem(LOCATION, state.mode.sitePermissions)
            }
            is QuickSettingsState.Mode.ActionLabelUpdated -> {
                bindPhoneFeatureItem(state.mode.phoneFeature, state.mode.sitePermissions)
            }
            is QuickSettingsState.Mode.CheckPendingFeatureBlockedByAndroid -> {
                checkFeaturesBlockedByAndroid(state.mode.sitePermissions)
            }
        }
    }

    private fun bindUrl(url: String) {
        this.url.text = url.toUri().hostWithoutCommonPrefixes
    }

    private fun bindTrackingProtectionInfo(isTrackingProtectionOn: Boolean) {
        val globalTPSetting = Settings.getInstance(context).shouldUseTrackingProtection
        val drawableId =
            if (isTrackingProtectionOn) R.drawable.ic_tracking_protection else
                R.drawable.ic_tracking_protection_disabled
        val icon = AppCompatResources.getDrawable(context, drawableId)
        tracking_protection.setCompoundDrawablesWithIntrinsicBounds(icon, null, null, null)
        tracking_protection.isChecked = isTrackingProtectionOn
        tracking_protection.isEnabled = globalTPSetting

        tracking_protection.setOnCheckedChangeListener { _, isChecked ->
            actionEmitter.onNext(
                QuickSettingsAction.ToggleTrackingProtection(
                    isChecked
                )
            )
        }
    }

    private fun bindTrackingProtectionAction() {
        val globalTPSetting = Settings.getInstance(context).shouldUseTrackingProtection
        tracking_protection_action.visibility = if (globalTPSetting) View.GONE else View.VISIBLE
        tracking_protection_action.setOnClickListener {
            actionEmitter.onNext(
                QuickSettingsAction.SelectTrackingProtectionSettings
            )
        }
    }

    private fun bindReportSiteIssueAction(url: String) {
        report_site_issue_action.setOnClickListener {
            actionEmitter.onNext(
                QuickSettingsAction.SelectReportProblem(url)
            )
        }
    }

    private fun bindSecurityInfo(isSecured: Boolean) {
        val stringId: Int
        val drawableId: Int
        val drawableTint: Int

        if (isSecured) {
            stringId = R.string.quick_settings_sheet_secure_connection
            drawableId = R.drawable.mozac_ic_lock
            drawableTint = R.color.photonGreen50
        } else {
            stringId = R.string.quick_settings_sheet_insecure_connection
            drawableId = R.drawable.mozac_ic_globe
            drawableTint = R.color.photonRed50
        }

        val icon = AppCompatResources.getDrawable(context, drawableId)
        icon?.setTint(ContextCompat.getColor(context, drawableTint))
        security_info.setText(stringId)
        security_info.setCompoundDrawablesWithIntrinsicBounds(icon, null, null, null)
    }

    private fun bindPhoneFeatureItem(phoneFeature: PhoneFeature, sitePermissions: SitePermissions? = null) {
        if (phoneFeature.shouldBeHidden(sitePermissions)) {
            hide(phoneFeature)
            return
        }
        show(phoneFeature)
        if (!phoneFeature.isAndroidPermissionGranted(context)) {
            handleBlockedByAndroidAction(phoneFeature)
        } else {
            bindPhoneAction(phoneFeature, sitePermissions)
        }
    }

    private fun show(phoneFeature: PhoneFeature) {
        val (label, action) = phoneFeature.labelAndAction
        label.visibility = VISIBLE
        action.visibility = VISIBLE
    }

    private fun hide(phoneFeature: PhoneFeature) {
        val (label, action) = phoneFeature.labelAndAction
        label.visibility = GONE
        action.visibility = GONE
    }

    private fun PhoneFeature.shouldBeHidden(sitePermissions: SitePermissions?): Boolean {
        return getStatus(sitePermissions, settings) == NO_DECISION
    }

    private fun PhoneFeature.isPermissionBlocked(sitePermissions: SitePermissions?): Boolean {
        return getStatus(sitePermissions, settings) == BLOCKED
    }

    private fun handleBlockedByAndroidAction(phoneFeature: PhoneFeature) {
        val (label, action) = phoneFeature.labelAndAction

        action.setText(R.string.phone_feature_blocked_by_android)
        action.tag = phoneFeature
        action.setOnClickListener {
            val feature = it.tag as PhoneFeature
            actionEmitter.onNext(
                QuickSettingsAction.SelectBlockedByAndroid(
                    feature.androidPermissionsList
                )
            )
        }
        label.setCompoundDrawablesWithIntrinsicBounds(phoneFeature.disabledIcon, null, null, null)
        label.isEnabled = false
        blockedByAndroidPhoneFeatures.add(phoneFeature)
    }

    private fun bindPhoneAction(phoneFeature: PhoneFeature, sitePermissions: SitePermissions? = null) {
        val (label, action) = phoneFeature.labelAndAction

        action.text = phoneFeature.getActionLabel(
            context = context,
            sitePermissions = sitePermissions,
            settings = settings
        )

        action.tag = phoneFeature
        action.setOnClickListener {
            val feature = it.tag as PhoneFeature
            actionEmitter.onNext(
                QuickSettingsAction.TogglePermission(feature)
            )
        }

        val icon = if (phoneFeature.isPermissionBlocked(sitePermissions)) {
            label.isEnabled = false
            phoneFeature.disabledIcon
        } else {
            label.isEnabled = true
            phoneFeature.enabledIcon
        }

        label.setCompoundDrawablesWithIntrinsicBounds(icon, null, null, null)
        blockedByAndroidPhoneFeatures.remove(phoneFeature)
    }

    private fun checkFeaturesBlockedByAndroid(sitePermissions: SitePermissions?) {
        val clonedList = blockedByAndroidPhoneFeatures.toTypedArray()
        clonedList.forEach { phoneFeature ->
            if (phoneFeature.isAndroidPermissionGranted(context)) {
                bindPhoneAction(phoneFeature, sitePermissions)
            }
        }
    }

    private val PhoneFeature.labelAndAction
        get(): Pair<TextView, TextView> {
            return when (this) {
                CAMERA -> camera_icon to camera_action_label
                LOCATION -> location_icon to location_action_label
                MICROPHONE -> microphone_icon to microphone_action_label
                NOTIFICATION -> notification_icon to notification_action_label
            }
        }

    private val PhoneFeature.enabledIcon
        get(): Drawable {
            val drawableId = when (this) {
                CAMERA -> R.drawable.ic_camera
                LOCATION -> R.drawable.ic_location
                MICROPHONE -> R.drawable.ic_microphone
                NOTIFICATION -> R.drawable.ic_notification
            }
            return requireNotNull(AppCompatResources.getDrawable(context, drawableId))
        }

    private val PhoneFeature.disabledIcon
        get(): Drawable {
            val drawableId = when (this) {
                CAMERA -> R.drawable.ic_camera_disabled
                LOCATION -> R.drawable.ic_location_disabled
                MICROPHONE -> R.drawable.ic_microphone_disabled
                NOTIFICATION -> R.drawable.ic_notifications_disabled
            }
            return requireNotNull(AppCompatResources.getDrawable(context, drawableId))
        }
}
