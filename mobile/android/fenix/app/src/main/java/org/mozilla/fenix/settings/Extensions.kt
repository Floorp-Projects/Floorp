/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings

import android.view.View
import android.widget.RadioButton
import android.widget.TextView
import androidx.appcompat.content.res.AppCompatResources
import androidx.core.text.HtmlCompat
import androidx.preference.Preference
import mozilla.components.feature.sitepermissions.SitePermissions
import mozilla.components.support.ktx.android.view.putCompoundDrawablesRelative
import org.mozilla.fenix.R
import org.mozilla.fenix.theme.ThemeManager

fun SitePermissions.toggle(featurePhone: PhoneFeature): SitePermissions {
    return when (featurePhone) {
        PhoneFeature.CAMERA -> copy(camera = camera.toggle())
        PhoneFeature.LOCATION -> copy(location = location.toggle())
        PhoneFeature.MICROPHONE -> copy(microphone = microphone.toggle())
        PhoneFeature.NOTIFICATION -> copy(notification = notification.toggle())
        PhoneFeature.AUTOPLAY_AUDIBLE -> copy(autoplayAudible = autoplayAudible.toggle())
        PhoneFeature.AUTOPLAY_INAUDIBLE -> copy(autoplayInaudible = autoplayInaudible.toggle())
    }
}

/**
 * In devices with Android 6, when we use android:button="@null" android:drawableStart doesn't work via xml
 * as a result we have to apply it programmatically. More info about this issue https://github.com/mozilla-mobile/fenix/issues/1414
 */
fun RadioButton.setStartCheckedIndicator() {
    val attr = ThemeManager.resolveAttribute(android.R.attr.listChoiceIndicatorSingle, context)
    val buttonDrawable = AppCompatResources.getDrawable(context, attr)
    buttonDrawable?.apply {
        setBounds(0, 0, intrinsicWidth, intrinsicHeight)
    }
    putCompoundDrawablesRelative(start = buttonDrawable)
}

fun initBlockedByAndroidView(phoneFeature: PhoneFeature, blockedByAndroidView: View) {
    val context = blockedByAndroidView.context
    if (!phoneFeature.isAndroidPermissionGranted(context)) {
        blockedByAndroidView.visibility = View.VISIBLE

        val descriptionLabel = blockedByAndroidView.findViewById<TextView>(R.id.blocked_by_android_feature_label)
        val descriptionText = context.getString(
            R.string.phone_feature_blocked_step_feature,
            phoneFeature.getLabel(context)
        )
        descriptionLabel.text = HtmlCompat.fromHtml(descriptionText, HtmlCompat.FROM_HTML_MODE_COMPACT)

        val permissionsLabel = blockedByAndroidView.findViewById<TextView>(R.id.blocked_by_android_permissions_label)
        val permissionsText = context.getString(R.string.phone_feature_blocked_step_permissions)
        permissionsLabel.text = HtmlCompat.fromHtml(permissionsText, HtmlCompat.FROM_HTML_MODE_COMPACT)
    } else {
        blockedByAndroidView.visibility = View.GONE
    }
}

/**
 * Sets the callback to be invoked when this preference is changed by the user (but before
 * the internal state has been updated). Allows the type of the preference to be specified.
 * If the new value doesn't match the preference type the listener isn't called.
 *
 * @param onPreferenceChangeListener The callback to be invoked
 */
inline fun <reified T> Preference.setOnPreferenceChangeListener(
    crossinline onPreferenceChangeListener: (Preference, T) -> Boolean
) {
    setOnPreferenceChangeListener { preference: Preference, newValue: Any ->
        (newValue as? T)?.let { onPreferenceChangeListener(preference, it) } ?: false
    }
}
