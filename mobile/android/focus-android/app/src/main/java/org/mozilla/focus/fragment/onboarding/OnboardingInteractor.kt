/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.fragment.onboarding

import android.content.Context
import androidx.preference.PreferenceManager
import org.mozilla.focus.state.AppAction
import org.mozilla.focus.state.AppStore

class OnboardingInteractor(val appStore: AppStore) {

    fun finishOnboarding(context: Context, selectedTabId: String?) {
        PreferenceManager.getDefaultSharedPreferences(context)
            .edit()
            .putBoolean(OnboardingFragment.ONBOARDING_PREF, true)
            .apply()

        appStore.dispatch(AppAction.FinishFirstRun(selectedTabId))
    }
}
