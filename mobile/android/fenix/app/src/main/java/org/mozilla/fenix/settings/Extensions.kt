/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings

import android.widget.RadioButton
import androidx.appcompat.content.res.AppCompatResources
import androidx.preference.Preference
import mozilla.components.feature.sitepermissions.SitePermissions
import mozilla.components.support.ktx.android.view.putCompoundDrawablesRelative
import org.mozilla.fenix.theme.ThemeManager

fun SitePermissions.toggle(featurePhone: PhoneFeature): SitePermissions {
    return update(featurePhone, get(featurePhone).toggle())
}

fun SitePermissions.get(field: PhoneFeature) = when (field) {
    PhoneFeature.CAMERA -> camera
    PhoneFeature.LOCATION -> location
    PhoneFeature.MICROPHONE -> microphone
    PhoneFeature.NOTIFICATION -> notification
    PhoneFeature.AUTOPLAY_AUDIBLE -> autoplayAudible
    PhoneFeature.AUTOPLAY_INAUDIBLE -> autoplayInaudible
}

fun SitePermissions.update(field: PhoneFeature, value: SitePermissions.Status) = when (field) {
    PhoneFeature.CAMERA -> copy(camera = value)
    PhoneFeature.LOCATION -> copy(location = value)
    PhoneFeature.MICROPHONE -> copy(microphone = value)
    PhoneFeature.NOTIFICATION -> copy(notification = value)
    PhoneFeature.AUTOPLAY_AUDIBLE -> copy(autoplayAudible = value)
    PhoneFeature.AUTOPLAY_INAUDIBLE -> copy(autoplayInaudible = value)
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
