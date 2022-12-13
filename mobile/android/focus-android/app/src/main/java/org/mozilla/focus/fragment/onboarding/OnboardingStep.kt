/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.fragment.onboarding

import org.mozilla.focus.R

enum class OnboardingStep(val prefId: Int) {
    ON_BOARDING_FIRST_SCREEN(R.string.pref_key_first_screen),
    ON_BOARDING_SECOND_SCREEN(R.string.pref_key_second_screen),
}
