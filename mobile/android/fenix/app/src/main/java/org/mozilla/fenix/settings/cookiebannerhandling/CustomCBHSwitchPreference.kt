/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.cookiebannerhandling

import android.content.Context
import android.util.AttributeSet
import androidx.preference.SwitchPreference
import org.mozilla.fenix.ext.settings

/**
 * Custom [SwitchPreference] that automatically creates the switch for the
 * cookie banner handling feature depending on the current Nimbus configurations.
 */
class CustomCBHSwitchPreference @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
) : SwitchPreference(context, attrs) {
    init {
        with(context) {
            setDefaultValue(settings().shouldUseCookieBannerPrivateModeDefaultValue)
        }
    }
}
