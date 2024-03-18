/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package mozilla.components.browser.engine.gecko.mediaquery

import mozilla.components.concept.engine.mediaquery.PreferredColorScheme
import org.mozilla.geckoview.GeckoRuntimeSettings

internal fun PreferredColorScheme.Companion.from(geckoValue: Int) =
    when (geckoValue) {
        GeckoRuntimeSettings.COLOR_SCHEME_DARK -> PreferredColorScheme.Dark
        GeckoRuntimeSettings.COLOR_SCHEME_LIGHT -> PreferredColorScheme.Light
        GeckoRuntimeSettings.COLOR_SCHEME_SYSTEM -> PreferredColorScheme.System
        else -> PreferredColorScheme.System
    }

internal fun PreferredColorScheme.toGeckoValue() =
    when (this) {
        is PreferredColorScheme.Dark -> GeckoRuntimeSettings.COLOR_SCHEME_DARK
        is PreferredColorScheme.Light -> GeckoRuntimeSettings.COLOR_SCHEME_LIGHT
        is PreferredColorScheme.System -> GeckoRuntimeSettings.COLOR_SCHEME_SYSTEM
    }
