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
import kotlinx.android.synthetic.main.fragment_add_on_settings.*
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.EngineView
import mozilla.components.feature.addons.AddOn
import org.mozilla.samples.browser.R
import org.mozilla.samples.browser.ext.components

/**
 * An activity to show the settings of an add-on.
 */
class AddOnSettingsActivity : AppCompatActivity() {

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_add_on_settings)

        val addOn = requireNotNull(intent.getParcelableExtra<AddOn>("add_on"))
        title = addOn.translatableName.translate()

        supportFragmentManager
            .beginTransaction()
            .replace(R.id.addOnSettingsContainer, AddOnSettingsFragment.create(addOn))
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
    class AddOnSettingsFragment : Fragment() {
        private lateinit var addOn: AddOn
        private lateinit var engineObserver: EngineSession.Observer
        private lateinit var engineSession: EngineSession

        override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?): View? {
            addOn = requireNotNull(arguments?.getParcelable("add_on"))
            engineSession = components.engine.createSession()
            engineObserver = object : EngineSession.Observer {
                override fun onLoadRequest(
                    url: String,
                    triggeredByRedirect: Boolean,
                    triggeredByWebContent: Boolean,
                    shouldLoadUri: (Boolean, String) -> Unit
                ) {
                    shouldLoadUri(true, "")
                }
            }
            engineSession.register(engineObserver)

            return inflater.inflate(R.layout.fragment_add_on_settings, container, false)
        }

        override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
            super.onViewCreated(view, savedInstanceState)

            addOnSettingsEngineView.render(engineSession)

            // update the url after add_on and web_extension link
            engineSession.loadUrl(addOn.siteUrl)
        }

        override fun onDestroyView() {
            engineSession.unregister(engineObserver)
            engineSession.close()
            super.onDestroyView()
        }

        companion object {
            /**
             * Create an [AddOnSettingsFragment] with add_on as a required parameter.
             */
            fun create(addOn: AddOn) = AddOnSettingsFragment().apply {
                arguments = Bundle().apply {
                    putParcelable("add_on", addOn)
                }
            }
        }
    }
}
