/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray.ext

import com.google.android.material.tabs.TabLayout
import org.mozilla.fenix.tabstray.TrayPagerAdapter

fun TabLayout.isNormalModeSelected(): Boolean {
    return selectedTabPosition == TrayPagerAdapter.POSITION_NORMAL_TABS
}

fun TabLayout.isPrivateModeSelected(): Boolean {
    return selectedTabPosition == TrayPagerAdapter.POSITION_PRIVATE_TABS
}

fun TabLayout.isSyncedModeSelected(): Boolean {
    return selectedTabPosition == TrayPagerAdapter.POSITION_SYNCED_TABS
}
