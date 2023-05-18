/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray

internal object TabsTrayTestTag {
    const val tabsTray = "tabstray"

    // Tabs Tray Banner
    private const val bannerTestTagRoot = "$tabsTray.banner"
    const val normalTabsPageButton = "$bannerTestTagRoot.normalTabsPageButton"
    const val privateTabsPageButton = "$bannerTestTagRoot.privateTabsPageButton"
    const val syncedTabsPageButton = "$bannerTestTagRoot.syncedTabsPageButton"

    // FAB
    const val fab = "$tabsTray.fab"

    // Tab lists
    private const val tabListTestTagRoot = "$tabsTray.tabList"
    const val normalTabsList = "$tabListTestTagRoot.normal"
    const val privateTabsList = "$tabListTestTagRoot.private"
    const val syncedTabsList = "$tabListTestTagRoot.synced"

    const val emptyNormalTabsList = "$normalTabsList.empty"
    const val emptyPrivateTabsList = "$privateTabsList.empty"

    // Tab items
    private const val tabItemRoot = "$tabsTray.tabItem"
    const val tabItemClose = "$tabItemRoot.close"
}
