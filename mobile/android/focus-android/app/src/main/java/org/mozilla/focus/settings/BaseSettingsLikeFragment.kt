/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.settings

import android.os.Bundle
import android.view.Menu
import android.view.MenuInflater
import android.view.MenuItem
import android.view.View
import androidx.core.view.MenuHost
import androidx.core.view.MenuProvider
import androidx.fragment.app.Fragment
import androidx.lifecycle.Lifecycle
import androidx.preference.PreferenceFragmentCompat
import org.mozilla.focus.R
import org.mozilla.focus.activity.MainActivity

/**
 * Similar behavior as [BaseSettingsFragment], but doesn't extend [PreferenceFragmentCompat] and is
 * a regular [Fragment] instead.
 */
open class BaseSettingsLikeFragment : Fragment(), MenuProvider {
    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        val menuHost: MenuHost = requireHost() as MenuHost
        menuHost.addMenuProvider(this, viewLifecycleOwner, Lifecycle.State.RESUMED)

        // Customize status bar background if the parent activity can be casted to MainActivity
        (requireActivity() as? MainActivity)?.customizeStatusBar(R.color.settings_background)
    }

    override fun onCreateMenu(menu: Menu, menuInflater: MenuInflater) {
        // no-op
    }

    override fun onMenuItemSelected(menuItem: MenuItem): Boolean = false
}
