/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.quicksettings

import android.view.View
import android.view.ViewGroup
import android.widget.TextView
import androidx.annotation.ColorRes
import androidx.annotation.DrawableRes
import androidx.annotation.IdRes
import androidx.annotation.StringRes
import androidx.core.content.ContextCompat
import androidx.core.net.toUri
import androidx.core.view.isVisible
import io.reactivex.Observable
import io.reactivex.Observer
import io.reactivex.functions.Consumer
import kotlinx.android.synthetic.main.fragment_quick_settings_dialog_sheet.*
import mozilla.components.feature.sitepermissions.SitePermissions
import mozilla.components.feature.sitepermissions.SitePermissions.Status.BLOCKED
import mozilla.components.feature.sitepermissions.SitePermissions.Status.NO_DECISION
import mozilla.components.support.ktx.android.net.hostWithoutCommonPrefixes
import mozilla.components.support.ktx.android.view.putCompoundDrawablesRelativeWithIntrinsicBounds
import org.mozilla.fenix.R
import org.mozilla.fenix.mvi.UIView
import org.mozilla.fenix.settings.PhoneFeature
import org.mozilla.fenix.settings.PhoneFeature.CAMERA
import org.mozilla.fenix.settings.PhoneFeature.LOCATION
import org.mozilla.fenix.settings.PhoneFeature.MICROPHONE
import org.mozilla.fenix.settings.PhoneFeature.NOTIFICATION
import org.mozilla.fenix.utils.Settings

typealias LabelActionPair = Pair<TextView, TextView>

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
    private inline val context get() = view.context
    private val settings: Settings = Settings.getInstance(context)
    private val trackingProtectionSettingView = TrackingProtectionSettingView(view, actionEmitter)
    private val labelAndActions = mapOf(
        CAMERA to findLabelActionPair(R.id.camera_icon, R.id.camera_action_label),
        LOCATION to findLabelActionPair(R.id.location_icon, R.id.location_action_label),
        MICROPHONE to findLabelActionPair(R.id.microphone_icon, R.id.microphone_action_label),
        NOTIFICATION to findLabelActionPair(R.id.notification_icon, R.id.notification_action_label)
    )

    private val blockedByAndroidClickListener = View.OnClickListener {
        val feature = it.tag as PhoneFeature
        actionEmitter.onNext(
            QuickSettingsAction.SelectBlockedByAndroid(feature.androidPermissionsList)
        )
    }
    private val togglePermissionClickListener = View.OnClickListener {
        val feature = it.tag as PhoneFeature
        actionEmitter.onNext(
            QuickSettingsAction.TogglePermission(feature)
        )
    }

    override fun updateView() = Consumer<QuickSettingsState> { state ->
        when (state.mode) {
            is QuickSettingsState.Mode.Normal -> {
                bindUrl(state.mode.url)
                bindSecurityInfo(state.mode.isSecured)
                bindReportSiteIssueAction(state.mode.url)
                trackingProtectionSettingView.bind(state.mode.isTrackingProtectionOn)
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

    private fun bindReportSiteIssueAction(url: String) {
        report_site_issue_action.setOnClickListener {
            actionEmitter.onNext(
                QuickSettingsAction.SelectReportProblem(url)
            )
        }
    }

    private fun bindSecurityInfo(isSecured: Boolean) {
        @StringRes val stringId: Int
        @DrawableRes val drawableId: Int
        @ColorRes val drawableTint: Int

        if (isSecured) {
            stringId = R.string.quick_settings_sheet_secure_connection
            drawableId = R.drawable.mozac_ic_lock
            drawableTint = R.color.photonGreen50
        } else {
            stringId = R.string.quick_settings_sheet_insecure_connection
            drawableId = R.drawable.mozac_ic_broken_lock
            drawableTint = R.color.photonRed50
        }

        val icon = context.getDrawable(drawableId)
        icon?.setTint(ContextCompat.getColor(context, drawableTint))
        security_info.setText(stringId)
        security_info.putCompoundDrawablesRelativeWithIntrinsicBounds(start = icon)
    }

    private fun bindPhoneFeatureItem(phoneFeature: PhoneFeature, sitePermissions: SitePermissions? = null) {
        val (label, action) = labelAndActions.getValue(phoneFeature)
        val shouldBeVisible = phoneFeature.shouldBeVisible(sitePermissions)
        label.isVisible = shouldBeVisible
        action.isVisible = shouldBeVisible

        if (shouldBeVisible) {
            if (phoneFeature.isAndroidPermissionGranted(context)) {
                bindPhoneAction(phoneFeature, sitePermissions)
            } else {
                handleBlockedByAndroidAction(phoneFeature)
            }
        }
    }

    private fun PhoneFeature.shouldBeVisible(sitePermissions: SitePermissions?): Boolean {
        return getStatus(sitePermissions, settings) != NO_DECISION
    }

    private fun PhoneFeature.isPermissionBlocked(sitePermissions: SitePermissions?): Boolean {
        return getStatus(sitePermissions, settings) == BLOCKED
    }

    private fun handleBlockedByAndroidAction(phoneFeature: PhoneFeature) {
        val (label, action) = labelAndActions.getValue(phoneFeature)

        action.setText(R.string.phone_feature_blocked_by_android)
        action.tag = phoneFeature
        action.setOnClickListener(blockedByAndroidClickListener)
        label.isEnabled = false
        blockedByAndroidPhoneFeatures.add(phoneFeature)
    }

    private fun bindPhoneAction(phoneFeature: PhoneFeature, sitePermissions: SitePermissions? = null) {
        val (label, action) = labelAndActions.getValue(phoneFeature)

        action.text = phoneFeature.getActionLabel(
            context = context,
            sitePermissions = sitePermissions,
            settings = settings
        )

        action.tag = phoneFeature
        action.setOnClickListener(togglePermissionClickListener)

        label.isEnabled = !phoneFeature.isPermissionBlocked(sitePermissions)
        blockedByAndroidPhoneFeatures.remove(phoneFeature)
    }

    private fun checkFeaturesBlockedByAndroid(sitePermissions: SitePermissions?) {
        blockedByAndroidPhoneFeatures.forEach { phoneFeature ->
            if (phoneFeature.isAndroidPermissionGranted(context)) {
                bindPhoneAction(phoneFeature, sitePermissions)
            }
        }
    }

    private fun findLabelActionPair(@IdRes labelId: Int, @IdRes actionId: Int): LabelActionPair {
        return view.findViewById<TextView>(labelId) to view.findViewById(actionId)
    }
}
