/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.activity

import android.os.Bundle
import android.view.MenuItem
import kotlinx.android.synthetic.main.activity_settings.*
import org.mozilla.focus.R
import org.mozilla.focus.locale.LocaleAwareAppCompatActivity
import org.mozilla.focus.settings.BaseSettingsFragment
import org.mozilla.focus.settings.SettingsFragment

class SettingsActivity : LocaleAwareAppCompatActivity(), BaseSettingsFragment.ActionBarUpdater {

    companion object {
        @JvmField
        val ACTIVITY_RESULT_LOCALE_CHANGED = 1
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        setContentView(R.layout.activity_settings)
        setSupportActionBar(toolbar)

        supportActionBar?.setDisplayHomeAsUpEnabled(true)

        if (savedInstanceState == null) {
            fragmentManager.beginTransaction()
                    .add(R.id.container, SettingsFragment.newInstance())
                    .commit()
        }

        // Ensure all locale specific Strings are initialised on first run, we don't set the title
        // anywhere before now (the title can only be set via AndroidManifest, and ensuring
        // that that loads the correct locale string is tricky).
        applyLocale()
    }

    override fun onOptionsItemSelected(item: MenuItem): Boolean = when (item.itemId) {
        android.R.id.home -> {
            onBackPressed()
            true
        }
        else -> super.onOptionsItemSelected(item)
    }

    override fun applyLocale() {
        setTitle(R.string.menu_settings)
    }

    override fun updateTitle(titleResId: Int) {
        setTitle(titleResId)
    }

    override fun updateIcon(iconResId: Int) {
        supportActionBar?.setHomeAsUpIndicator(iconResId)
    }
}
