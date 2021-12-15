/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.settings

import android.content.Context
import android.util.AttributeSet
import mozilla.components.concept.engine.Engine
import org.mozilla.focus.ext.components
import org.mozilla.focus.utils.Features

/**
 * Preference for HTTPS-Only mode.
 */
class HttpsOnlyModePreference(
    context: Context,
    attrs: AttributeSet?
) : LearnMoreSwitchPreference(context, attrs) {
    // Replace with SUMO link for Focus (This is the desktop link)
    // https://github.com/mozilla-mobile/focus-android/issues/6008
    override fun getLearnMoreUrl() = "https://support.mozilla.org/en-US/kb/https-only-prefs"

    init {
        if (!Features.HTTPS_ONLY_MODE) {
            isVisible = false
        }

        setOnPreferenceChangeListener { _, newValue ->
            val enableHttpsOnly = newValue as Boolean
            context.components.engine.settings.httpsOnlyMode = if (enableHttpsOnly) {
                Engine.HttpsOnlyMode.ENABLED
            } else {
                Engine.HttpsOnlyMode.DISABLED
            }
            true
        }
    }
}
