/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.fenix.browser

import android.os.Bundle
import android.transition.TransitionInflater
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.fragment.app.Fragment
import kotlinx.android.synthetic.main.fragment_browser.*
import mozilla.components.feature.session.SessionFeature
import mozilla.components.feature.session.SessionUseCases
import mozilla.components.support.ktx.android.arch.lifecycle.addObservers
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R

class BrowserFragment : Fragment() {

    private lateinit var sessionFeature: SessionFeature

    override fun onCreateView(
        inflater: LayoutInflater, container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View? {
        return inflater.inflate(R.layout.fragment_browser, container, false)
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        sharedElementEnterTransition = TransitionInflater.from(context).inflateTransition(android.R.transition.move)
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)


        val sessionManager = (activity as HomeActivity).core.sessionManager
        val sessionId = (activity as HomeActivity).sessionId

        sessionFeature = SessionFeature(
            sessionManager,
            SessionUseCases(sessionManager),
            engineView,
            sessionId)

        lifecycle.addObservers(sessionFeature)
    }
}
