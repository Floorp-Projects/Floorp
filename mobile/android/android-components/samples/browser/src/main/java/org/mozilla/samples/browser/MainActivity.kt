/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.samples.browser

import android.content.Context
import android.os.Bundle
import android.support.v7.app.AppCompatActivity
import android.util.AttributeSet
import android.view.View
import kotlinx.android.synthetic.main.activity_main.*
import mozilla.components.concept.engine.EngineView
import mozilla.components.feature.session.SessionFeature
import mozilla.components.feature.toolbar.ToolbarFeature

class MainActivity : AppCompatActivity() {
    private var sessionFeature : SessionFeature? = null
    private var toolbarFeature : ToolbarFeature? = null

    private val components by lazy {
        Components(applicationContext)
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        sessionFeature = SessionFeature(components.sessionManager, components.engine, engineView,
                components.sessionMapping)

        toolbarFeature = ToolbarFeature(components.sessionManager, components.sessionUseCases.loadUrl, toolbar)

        components.sessionIntentProcessor.process(intent)
    }

    override fun onResume() {
        super.onResume()

        sessionFeature?.start()
        toolbarFeature?.start()
    }

    override fun onPause() {
        super.onPause()

        sessionFeature?.stop()
        toolbarFeature?.stop()
    }

    override fun onCreateView(parent: View?, name: String?, context: Context?, attrs: AttributeSet?): View? {
        if (name == EngineView::class.java.name) {
            return components.engine.createView(context!!, attrs).asView()
        }

        return super.onCreateView(parent, name, context, attrs)
    }
}
