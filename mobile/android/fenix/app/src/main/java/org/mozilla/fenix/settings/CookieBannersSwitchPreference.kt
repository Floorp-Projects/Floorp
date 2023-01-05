/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings

import android.content.Context
import android.util.AttributeSet
import org.mozilla.fenix.ext.settings

/**
 * Cookie banners switch preference with a learn more link.
 */
class CookieBannersSwitchPreference(context: Context, attrs: AttributeSet?) :
    LearnMoreSwitchPreference(context, attrs) {

    override fun getLearnMoreUrl(): String {
        return SupportUtils.getGenericSumoURLForTopic(
            SupportUtils.SumoTopic.COOKIE_BANNER,
        )
    }

    override fun getSwitchValue(): Boolean {
        return context.settings().shouldUseCookieBanner
    }
}
