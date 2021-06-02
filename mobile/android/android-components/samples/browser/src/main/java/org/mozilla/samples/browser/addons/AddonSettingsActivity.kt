/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.samples.browser.addons

import android.content.Context
import android.os.Bundle
import android.util.AttributeSet
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.appcompat.app.AppCompatActivity
import androidx.fragment.app.Fragment
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.EngineView
import mozilla.components.feature.addons.Addon
import mozilla.components.feature.addons.ui.translateName
import org.mozilla.samples.browser.R
import org.mozilla.samples.browser.databinding.ActivityAddOnSettingsBinding
import org.mozilla.samples.browser.databinding.FragmentAddOnSettingsBinding
import org.mozilla.samples.browser.ext.components

/**
 * An activity to show the settings of an add-on.
 */
class AddonSettingsActivity : AppCompatActivity() {

    private lateinit var binding: ActivityAddOnSettingsBinding

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        binding = ActivityAddOnSettingsBinding.inflate(layoutInflater)
        val view = binding.root
        setContentView(view)

        val addon = requireNotNull(intent.getParcelableExtra<Addon>("add_on"))
        title = addon.translateName(this)

        supportFragmentManager
            .beginTransaction()
            .replace(R.id.addonSettingsContainer, AddonSettingsFragment.create(addon))
            .commit()
    }

    override fun onCreateView(parent: View?, name: String, context: Context, attrs: AttributeSet): View? =
        when (name) {
            EngineView::class.java.name -> components.engine.createView(context, attrs).asView()
            else -> super.onCreateView(parent, name, context, attrs)
        }

    /**
     * A fragment to show the settings of an add-on with [EngineView].
     */
    class AddonSettingsFragment : Fragment() {
        private lateinit var addon: Addon
        private lateinit var engineSession: EngineSession

        override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?): View? {
            addon = requireNotNull(arguments?.getParcelable("add_on"))
            engineSession = components.engine.createSession()

            return inflater.inflate(R.layout.fragment_add_on_settings, container, false)
        }

        override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
            super.onViewCreated(view, savedInstanceState)

            val binding = FragmentAddOnSettingsBinding.bind(view)
            binding.addonSettingsEngineView.render(engineSession)
            addon.installedState?.optionsPageUrl?.let {
                engineSession.loadUrl(it)
            }
        }

        override fun onDestroyView() {
            engineSession.close()
            super.onDestroyView()
        }

        companion object {
            /**
             * Create an [AddonSettingsFragment] with add_on as a required parameter.
             */
            fun create(addon: Addon) = AddonSettingsFragment().apply {
                arguments = Bundle().apply {
                    putParcelable("add_on", addon)
                }
            }
        }
    }
}
