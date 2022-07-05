/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.settings

import android.os.Bundle
import android.view.View
import androidx.fragment.app.Fragment
import androidx.preference.PreferenceFragmentCompat
import org.mozilla.focus.R
import org.mozilla.focus.activity.MainActivity

/**
 * Similar behavior as [BaseSettingsFragment], but doesn't extend [PreferenceFragmentCompat] and is
 * a regular [Fragment] instead.
 */
open class BaseSettingsLikeFragment : Fragment() {
    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        // Customize status bar background if the parent activity can be casted to MainActivity
        (requireActivity() as? MainActivity)?.customizeStatusBar(R.color.settings_background)
    }
}
