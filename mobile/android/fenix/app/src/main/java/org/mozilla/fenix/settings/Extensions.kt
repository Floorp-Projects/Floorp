/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings

import android.widget.RadioButton
import androidx.annotation.StringRes
import androidx.appcompat.content.res.AppCompatResources
import androidx.preference.Preference
import androidx.preference.PreferenceFragmentCompat
import mozilla.components.concept.engine.permission.SitePermissions
import mozilla.components.support.ktx.android.content.res.resolveAttribute
import mozilla.components.support.ktx.android.view.putCompoundDrawablesRelative
import org.mozilla.fenix.ext.getPreferenceKey

fun SitePermissions.toggle(featurePhone: PhoneFeature): SitePermissions {
    return update(featurePhone, get(featurePhone).toggle())
}

fun SitePermissions.get(field: PhoneFeature) = when (field) {
    PhoneFeature.AUTOPLAY ->
        throw IllegalAccessException("AUTOPLAY can't be accessed via get try " +
                "using AUTOPLAY_AUDIBLE and AUTOPLAY_INAUDIBLE")
    PhoneFeature.CAMERA -> camera
    PhoneFeature.LOCATION -> location
    PhoneFeature.MICROPHONE -> microphone
    PhoneFeature.NOTIFICATION -> notification
    PhoneFeature.AUTOPLAY_AUDIBLE -> autoplayAudible.toStatus()
    PhoneFeature.AUTOPLAY_INAUDIBLE -> autoplayInaudible.toStatus()
    PhoneFeature.PERSISTENT_STORAGE -> localStorage
    PhoneFeature.MEDIA_KEY_SYSTEM_ACCESS -> mediaKeySystemAccess
}

fun SitePermissions.update(field: PhoneFeature, value: SitePermissions.Status) = when (field) {
    PhoneFeature.AUTOPLAY -> throw IllegalAccessException("AUTOPLAY can't be accessed via update " +
            "try using AUTOPLAY_AUDIBLE and AUTOPLAY_INAUDIBLE")
    PhoneFeature.CAMERA -> copy(camera = value)
    PhoneFeature.LOCATION -> copy(location = value)
    PhoneFeature.MICROPHONE -> copy(microphone = value)
    PhoneFeature.NOTIFICATION -> copy(notification = value)
    PhoneFeature.AUTOPLAY_AUDIBLE -> copy(autoplayAudible = value.toAutoplayStatus())
    PhoneFeature.AUTOPLAY_INAUDIBLE -> copy(autoplayInaudible = value.toAutoplayStatus())
    PhoneFeature.PERSISTENT_STORAGE -> copy(localStorage = value)
    PhoneFeature.MEDIA_KEY_SYSTEM_ACCESS -> copy(mediaKeySystemAccess = value)
}

/**
 * In devices with Android 6, when we use android:button="@null" android:drawableStart doesn't work via xml
 * as a result we have to apply it programmatically. More info about this issue https://github.com/mozilla-mobile/fenix/issues/1414
 */
fun RadioButton.setStartCheckedIndicator() {
    val attr = context.theme.resolveAttribute(android.R.attr.listChoiceIndicatorSingle)
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

/**
 * Find a preference with the corresponding key and throw if it does not exist.
 * @param preferenceId Resource ID from preference_keys
 */
fun <T : Preference> PreferenceFragmentCompat.requirePreference(@StringRes preferenceId: Int) =
    requireNotNull(findPreference<T>(getPreferenceKey(preferenceId)))
