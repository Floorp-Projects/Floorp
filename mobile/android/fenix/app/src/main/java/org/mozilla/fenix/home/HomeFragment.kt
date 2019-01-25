/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home

import android.os.Bundle
import android.transition.TransitionInflater
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup

import androidx.core.view.ViewCompat
import androidx.fragment.app.Fragment
import androidx.navigation.Navigation
import androidx.navigation.fragment.FragmentNavigator
import androidx.recyclerview.widget.LinearLayoutManager
import kotlinx.android.synthetic.main.fragment_home.*
import org.mozilla.fenix.R
import org.mozilla.fenix.home.sessions.SessionsAdapter

class HomeFragment : Fragment() {
    override fun onCreateView(
        inflater: LayoutInflater, container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View? {
        return inflater.inflate(R.layout.fragment_home, container, false)
    }

    private lateinit var sessionsAdapter: SessionsAdapter

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        // Hide buttons that aren't used yet to prevent confusion
        menuButton.visibility = View.GONE
        privateBrowsingButton.visibility = View.GONE

        sessionsAdapter = SessionsAdapter()

        toolbar_wrapper.clipToOutline = false
        toolbar.setOnClickListener { it ->
            val extras = FragmentNavigator.Extras.Builder().addSharedElement(
                toolbar, ViewCompat.getTransitionName(toolbar)!!
            ).build()
            Navigation.findNavController(it).navigate(R.id.action_homeFragment_to_searchFragment, null, null, extras)
        }

        session_list.apply {
            adapter = sessionsAdapter
            layoutManager = LinearLayoutManager(requireContext())
            setHasFixedSize(true)
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enterTransition = TransitionInflater.from(context).inflateTransition(android.R.transition.fade)
        exitTransition = TransitionInflater.from(context).inflateTransition(android.R.transition.fade)
    }
}
