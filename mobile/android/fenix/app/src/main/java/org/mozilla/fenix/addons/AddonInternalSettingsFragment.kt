/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.addons

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.fragment.app.Fragment
import kotlinx.android.synthetic.main.fragment_add_on_internal_settings.*
import mozilla.components.concept.engine.EngineSession
import mozilla.components.feature.addons.Addon
import mozilla.components.feature.addons.ui.translate
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.requireComponents
import org.mozilla.fenix.ext.showToolbar

/**
 * A fragment to show the internal settings of an add-on.
 */
class AddonInternalSettingsFragment : Fragment() {
    private val addon: Addon by lazy {
        AddonDetailsFragmentArgs.fromBundle(requireNotNull(arguments)).addon
    }
    private lateinit var engineSession: EngineSession

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View? {
        engineSession = requireComponents.core.engine.createSession()

        return inflater.inflate(R.layout.fragment_add_on_internal_settings, container, false)
    }

    override fun onResume() {
        super.onResume()
        showToolbar(addon.translatableName.translate())
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        addonSettingsEngineView.render(engineSession)
        engineSession.loadUrl(addon.installedState!!.optionsPageUrl)
    }

    override fun onDestroyView() {
        engineSession.close()
        super.onDestroyView()
    }
}
