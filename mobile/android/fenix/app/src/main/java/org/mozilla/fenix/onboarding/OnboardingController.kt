/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.onboarding

import android.content.Context
import org.mozilla.fenix.BrowserDirection
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.settings.SupportUtils

class OnboardingController(
    private val context: Context
) {
    fun handleLearnMoreClicked() {
        (context as HomeActivity).openToBrowserAndLoad(
            searchTermOrURL = SupportUtils.getFirefoxAccountSumoUrl(),
            newTab = true,
            from = BrowserDirection.FromHome
        )
    }
}
