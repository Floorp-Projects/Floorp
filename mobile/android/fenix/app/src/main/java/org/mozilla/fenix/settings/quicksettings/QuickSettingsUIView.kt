/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.quicksettings

import android.util.TypedValue
import android.view.View
import android.view.ViewGroup
import android.widget.TextView
import androidx.appcompat.content.res.AppCompatResources
import androidx.appcompat.widget.AppCompatTextView
import androidx.core.content.ContextCompat
import io.reactivex.Observable
import io.reactivex.Observer
import io.reactivex.functions.Consumer
import mozilla.components.support.ktx.android.net.hostWithoutCommonPrefixes
import mozilla.components.support.ktx.kotlin.toUri
import org.jetbrains.anko.textColorResource
import org.mozilla.fenix.R
import org.mozilla.fenix.mvi.UIView
import org.mozilla.fenix.settings.PhoneFeature
import org.mozilla.fenix.settings.PhoneFeature.CAMERA
import org.mozilla.fenix.settings.PhoneFeature.LOCATION
import org.mozilla.fenix.settings.PhoneFeature.MICROPHONE
import org.mozilla.fenix.settings.PhoneFeature.NOTIFICATION
import org.mozilla.fenix.utils.ItsNotBrokenSnack
import org.mozilla.fenix.utils.Settings

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
    private val cameraActionLabel: TextView
    private val microphoneActionLabel: TextView
    private val locationActionLabel: TextView
    private val notificationActionLabel: TextView
    private val blockedByAndroidPhoneFeatures = mutableListOf<PhoneFeature>()
    private val context get() = view.context
    private val settings: Settings = Settings.getInstance(context)

    private val toolbarTextColorId by lazy {
        val typedValue = TypedValue()
        context.theme.resolveAttribute(R.attr.toolbarTextColor, typedValue, true)
        typedValue.resourceId
    }

    init {
        urlLabel = view.findViewById<AppCompatTextView>(R.id.url)
        securityInfoLabel = view.findViewById<AppCompatTextView>(R.id.security_info)
        cameraActionLabel = view.findViewById<AppCompatTextView>(R.id.camera_action_label)
        microphoneActionLabel = view.findViewById<AppCompatTextView>(R.id.microphone_action_label)
        locationActionLabel = view.findViewById<AppCompatTextView>(R.id.location_action_label)
        notificationActionLabel = view.findViewById<AppCompatTextView>(R.id.notification_action_label)
    }

    override fun updateView() = Consumer<QuickSettingsState> { state ->
        when (state.mode) {
            is QuickSettingsState.Mode.Normal -> {
                bindUrl(state.mode.url)
                bindSecurityInfo(state.mode.isSecured)
                bindPhoneFeatureItem(cameraActionLabel, CAMERA)
                bindPhoneFeatureItem(microphoneActionLabel, MICROPHONE)
                bindPhoneFeatureItem(notificationActionLabel, NOTIFICATION)
                bindPhoneFeatureItem(locationActionLabel, LOCATION)
                bindManagePermissionsButton()
            }
            is QuickSettingsState.Mode.ActionLabelUpdated -> {
                bindPhoneFeatureItem(
                    state.mode.phoneFeature.actionLabel,
                    state.mode.phoneFeature
                )
            }
            is QuickSettingsState.Mode.CheckPendingFeatureBlockedByAndroid -> {
                checkFeaturesBlockedByAndroid()
            }
        }
    }

    private fun bindUrl(url: String) {
        urlLabel.text = url.toUri().hostWithoutCommonPrefixes
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

    private fun bindPhoneFeatureItem(actionLabel: TextView, phoneFeature: PhoneFeature) {
        if (!phoneFeature.isAndroidPermissionGranted(context)) {
            handleBlockedByAndroidAction(actionLabel, phoneFeature)
        } else {
            bindPhoneAction(actionLabel, phoneFeature)
        }
    }

    private fun handleBlockedByAndroidAction(actionLabel: TextView, phoneFeature: PhoneFeature) {
        actionLabel.setText(R.string.phone_feature_blocked_by_android)
        actionLabel.setTextColor(ContextCompat.getColor(context, R.color.photonBlue50))
        actionLabel.tag = phoneFeature
        actionLabel.setOnClickListener {
            val feature = it.tag as PhoneFeature
            actionEmitter.onNext(
                QuickSettingsAction.SelectBlockedByAndroid(
                    feature.androidPermissionsList
                )
            )
        }
        blockedByAndroidPhoneFeatures.add(phoneFeature)
    }

    private fun bindPhoneAction(actionLabel: TextView, phoneFeature: PhoneFeature) {
        actionLabel.text = phoneFeature.getActionLabel(context = context, settings = settings)
        actionLabel.textColorResource = toolbarTextColorId
        actionLabel.isEnabled = false
        blockedByAndroidPhoneFeatures.remove(phoneFeature)
    }

    private fun bindManagePermissionsButton() {
        val urlLabel = view.findViewById<TextView>(R.id.manage_site_permissions)
        urlLabel.setOnClickListener {
            actionEmitter.onNext(QuickSettingsAction.DismissDialog)
            ItsNotBrokenSnack(context).showSnackbar(issueNumber = "1170")
        }
    }

    private fun checkFeaturesBlockedByAndroid() {
        val clonedList = blockedByAndroidPhoneFeatures.toTypedArray()
        clonedList.forEach { phoneFeature ->
            if (phoneFeature.isAndroidPermissionGranted(context)) {
                val actionLabel = phoneFeature.actionLabel
                bindPhoneAction(actionLabel, phoneFeature)
            }
        }
    }

    private val PhoneFeature.actionLabel
        get(): TextView {
            return when (this) {
                CAMERA -> cameraActionLabel
                LOCATION -> locationActionLabel
                MICROPHONE -> microphoneActionLabel
                NOTIFICATION -> notificationActionLabel
            }
        }
}
