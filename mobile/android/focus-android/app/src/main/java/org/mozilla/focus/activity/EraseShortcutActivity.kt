/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.activity

import android.app.Activity
import android.os.Bundle
import mozilla.components.browser.state.selector.privateTabs
import org.mozilla.focus.GleanMetrics.AppShortcuts
import org.mozilla.focus.ext.components

class EraseShortcutActivity : Activity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        components.tabsUseCases.removeAllTabs()

        val tabCount = components.store.state.privateTabs.size
        AppShortcuts.justEraseButtonTapped.record(AppShortcuts.JustEraseButtonTappedExtra(tabCount))

        finishAndRemoveTask()
    }
}
