/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.activity

import android.app.Activity
import android.content.Intent
import android.os.Bundle
import mozilla.components.browser.state.selector.privateTabs
import org.mozilla.focus.GleanMetrics.AppShortcuts
import org.mozilla.focus.ext.components

class EraseAndOpenShortcutActivity : Activity() {

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        components.tabsUseCases.removeAllTabs()

        val tabCount = components.store.state.privateTabs.size
        AppShortcuts.eraseOpenButtonTapped.record(AppShortcuts.EraseOpenButtonTappedExtra(tabCount))

        val intent = Intent(this, MainActivity::class.java)
        intent.action = MainActivity.ACTION_OPEN
        intent.flags = Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_CLEAR_TOP
        startActivity(intent)

        finish()
    }
}
