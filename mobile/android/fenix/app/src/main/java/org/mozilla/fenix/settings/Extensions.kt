/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings

import android.content.Context
import android.view.View
import android.widget.RadioButton
import android.widget.TextView
import androidx.core.text.HtmlCompat
import mozilla.components.feature.sitepermissions.SitePermissions
import mozilla.components.feature.sitepermissions.SitePermissionsRules
import mozilla.components.support.ktx.android.view.putCompoundDrawablesRelative
import org.mozilla.fenix.R
import org.mozilla.fenix.ThemeManager

internal fun SitePermissionsRules.Action.toString(context: Context): String {
    return when (this) {
        SitePermissionsRules.Action.ASK_TO_ALLOW -> {
            context.getString(R.string.preference_option_phone_feature_ask_to_allow)
        }
        SitePermissionsRules.Action.BLOCKED -> {
            context.getString(R.string.preference_option_phone_feature_blocked)
        }
    }
}

internal fun SitePermissions.Status.toString(context: Context): String {
    return when (this) {
        SitePermissions.Status.BLOCKED -> {
            context.getString(R.string.preference_option_phone_feature_blocked)
        }
        SitePermissions.Status.NO_DECISION -> {
            context.getString(R.string.preference_option_phone_feature_ask_to_allow)
        }
        SitePermissions.Status.ALLOWED -> {
            context.getString(R.string.preference_option_phone_feature_allowed)
        }
    }
}

fun SitePermissions.toggle(featurePhone: PhoneFeature): SitePermissions {
    return when (featurePhone) {
        PhoneFeature.CAMERA -> {
            copy(
                camera = camera.toggle()
            )
        }
        PhoneFeature.LOCATION -> {
            copy(
                location = location.toggle()
            )
        }
        PhoneFeature.MICROPHONE -> {
            copy(
                microphone = microphone.toggle()
            )
        }
        PhoneFeature.NOTIFICATION -> {
            copy(
                notification = notification.toggle()
            )
        }
    }
}

fun PhoneFeature.getLabel(context: Context): String {
    return when (this) {
        PhoneFeature.CAMERA -> context.getString(R.string.preference_phone_feature_camera)
        PhoneFeature.LOCATION -> context.getString(R.string.preference_phone_feature_location)
        PhoneFeature.MICROPHONE -> context.getString(R.string.preference_phone_feature_microphone)
        PhoneFeature.NOTIFICATION -> context.getString(R.string.preference_phone_feature_notification)
    }
}

fun PhoneFeature.getPreferenceKey(context: Context): String {
    return when (this) {
        PhoneFeature.CAMERA -> context.getString(R.string.pref_key_phone_feature_camera)
        PhoneFeature.LOCATION -> context.getString(R.string.pref_key_phone_feature_location)
        PhoneFeature.MICROPHONE -> context.getString(R.string.pref_key_phone_feature_microphone)
        PhoneFeature.NOTIFICATION -> context.getString(R.string.pref_key_phone_feature_notification)
    }
}

/**
 * In devices with Android 6, when we use android:button="@null" android:drawableStart doesn't work via xml
 * as a result we have to apply it programmatically. More info about this issue https://github.com/mozilla-mobile/fenix/issues/1414
 */
fun RadioButton.setStartCheckedIndicator() {
    val attr = ThemeManager.resolveAttribute(android.R.attr.listChoiceIndicatorSingle, context)
    val buttonDrawable = context.getDrawable(attr)
    buttonDrawable?.apply {
        setBounds(0, 0, intrinsicWidth, intrinsicHeight)
    }
    putCompoundDrawablesRelative(start = buttonDrawable)
}

fun initBlockedByAndroidView(phoneFeature: PhoneFeature, blockedByAndroidView: View) {
    val context = blockedByAndroidView.context
    if (!phoneFeature.isAndroidPermissionGranted(context)) {
        blockedByAndroidView.visibility = View.VISIBLE

        val descriptionLabel = blockedByAndroidView.findViewById<TextView>(R.id.blocked_by_android_explanation_label)
        val text = context.getString(
            R.string.phone_feature_blocked_by_android_explanation,
            phoneFeature.getLabel(context)
        )
        descriptionLabel.text = HtmlCompat.fromHtml(text, HtmlCompat.FROM_HTML_MODE_COMPACT)
    } else {
        blockedByAndroidView.visibility = View.GONE
    }
}
