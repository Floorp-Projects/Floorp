/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.cookiebanner

import mozilla.components.concept.engine.EngineSession
import org.mozilla.focus.R

sealed class CookieBannerOption(
    open val prefKeyId: Int,
    open val mode: EngineSession.CookieBannerHandlingMode,
    open val metricTag: String,
) {

    data class CookieBannerRejectAll(
        override val prefKeyId: Int = R.string.pref_key_cookie_banner_reject_all,
        override val mode: EngineSession.CookieBannerHandlingMode =
            EngineSession.CookieBannerHandlingMode.REJECT_ALL,
        override val metricTag: String = "reject_all",
    ) : CookieBannerOption(prefKeyId = prefKeyId, mode = mode, metricTag = metricTag)

    data class CookieBannerDisabled(
        override val prefKeyId: Int = R.string.pref_key_cookie_banner_disabled,
        override val mode: EngineSession.CookieBannerHandlingMode =
            EngineSession.CookieBannerHandlingMode.DISABLED,
        override val metricTag: String = "disabled",
    ) : CookieBannerOption(prefKeyId = prefKeyId, mode = mode, metricTag = metricTag)
}
