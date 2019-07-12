/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.quicksettings

import android.graphics.drawable.Drawable
import android.view.View
import android.view.View.GONE
import android.view.View.VISIBLE
import android.view.ViewGroup
import android.widget.Switch
import android.widget.TextView
import androidx.appcompat.content.res.AppCompatResources
import androidx.appcompat.widget.AppCompatTextView
import androidx.core.content.ContextCompat
import androidx.core.net.toUri
import io.reactivex.Observable
import io.reactivex.Observer
import io.reactivex.functions.Consumer
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
    private val securityInfoLabel: TextView
    private val urlLabel: TextView
    private val trackingProtectionSwitch: Switch
    private val trackingProtectionAction: TextView
    private val reportSiteIssueAction: TextView
    private val cameraActionLabel: TextView
    private val cameraLabel: TextView
    private val microphoneActionLabel: TextView
    private val microphoneLabel: TextView
    private val locationActionLabel: TextView
    private val locationLabel: TextView
    private val notificationActionLabel: TextView
    private val notificationLabel: TextView
    private val blockedByAndroidPhoneFeatures = mutableListOf<PhoneFeature>()
    private val context get() = view.context
    private val settings: Settings = Settings.getInstance(context)

    init {
        urlLabel = view.findViewById<AppCompatTextView>(R.id.url)
        securityInfoLabel = view.findViewById<AppCompatTextView>(R.id.security_info)
        trackingProtectionSwitch = view.findViewById(R.id.tracking_protection)
        trackingProtectionAction = view.findViewById(R.id.tracking_protection_action)
        reportSiteIssueAction = view.findViewById(R.id.report_site_issue_action)
        cameraActionLabel = view.findViewById<AppCompatTextView>(R.id.camera_action_label)
        cameraLabel = view.findViewById<AppCompatTextView>(R.id.camera_icon)
        microphoneActionLabel = view.findViewById<AppCompatTextView>(R.id.microphone_action_label)
        microphoneLabel = view.findViewById<AppCompatTextView>(R.id.microphone_icon)
        locationLabel = view.findViewById<AppCompatTextView>(R.id.location_icon)
        locationActionLabel = view.findViewById<AppCompatTextView>(R.id.location_action_label)
        notificationActionLabel = view.findViewById<AppCompatTextView>(R.id.notification_action_label)
        notificationLabel = view.findViewById<AppCompatTextView>(R.id.notification_icon)
    }

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
        urlLabel.text = url.toUri().hostWithoutCommonPrefixes
    }

    private fun bindTrackingProtectionInfo(isTrackingProtectionOn: Boolean) {
        val globalTPSetting = Settings.getInstance(context).shouldUseTrackingProtection
        val drawableId =
            if (isTrackingProtectionOn) R.drawable.ic_tracking_protection else
                R.drawable.ic_tracking_protection_disabled
        val icon = AppCompatResources.getDrawable(context, drawableId)
        trackingProtectionSwitch.setCompoundDrawablesWithIntrinsicBounds(icon, null, null, null)
        trackingProtectionSwitch.isChecked = isTrackingProtectionOn
        trackingProtectionSwitch.isEnabled = globalTPSetting

        trackingProtectionSwitch.setOnCheckedChangeListener { _, isChecked ->
            actionEmitter.onNext(
                QuickSettingsAction.ToggleTrackingProtection(
                    isChecked
                )
            )
        }
    }

    private fun bindTrackingProtectionAction() {
        val globalTPSetting = Settings.getInstance(context).shouldUseTrackingProtection
        trackingProtectionAction.visibility = if (globalTPSetting) View.GONE else View.VISIBLE
        trackingProtectionAction.setOnClickListener {
            actionEmitter.onNext(
                QuickSettingsAction.SelectTrackingProtectionSettings
            )
        }
    }

    private fun bindReportSiteIssueAction(url: String) {
        reportSiteIssueAction.setOnClickListener {
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
        securityInfoLabel.setText(stringId)
        securityInfoLabel.setCompoundDrawablesWithIntrinsicBounds(icon, null, null, null)
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
                CAMERA -> cameraLabel to cameraActionLabel
                LOCATION -> locationLabel to locationActionLabel
                MICROPHONE -> microphoneLabel to microphoneActionLabel
                NOTIFICATION -> notificationLabel to notificationActionLabel
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
