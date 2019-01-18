/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix

import android.content.Context
import android.os.Bundle
import android.util.AttributeSet
import android.view.View
import androidx.appcompat.app.AppCompatActivity
import mozilla.components.concept.engine.EngineView
import mozilla.components.feature.intent.IntentProcessor
import mozilla.components.support.utils.SafeIntent
import org.mozilla.fenix.components.Core

class HomeActivity : AppCompatActivity() {

    val core by lazy { Core(this) }
    val sessionId by lazy {
        SafeIntent(intent).getStringExtra(IntentProcessor.ACTIVE_SESSION_ID)
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_home)
    }

    override fun onCreateView(parent: View?, name: String?, context: Context, attrs: AttributeSet?): View? =
        when (name) {
            EngineView::class.java.name -> core.engine.createView(context, attrs).asView()
            else -> super.onCreateView(parent, name, context, attrs)
        }
}
