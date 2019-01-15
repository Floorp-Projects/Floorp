/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home

import android.content.Context
import android.graphics.Color
import android.os.Build
import android.os.Bundle
import android.transition.TransitionInflater
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.RelativeLayout
import android.widget.TextView
import androidx.annotation.RequiresApi
import androidx.core.view.ViewCompat
import androidx.fragment.app.Fragment
import androidx.navigation.Navigation
import androidx.navigation.fragment.FragmentNavigator
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import kotlinx.android.synthetic.main.fragment_home.*
import org.mozilla.fenix.R

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

        sessionsAdapter = SessionsAdapter(requireContext())

        toolbar_wrapper.clipToOutline = false
        toolbar.setOnClickListener { it ->
            val extras = FragmentNavigator.Extras.Builder().addSharedElement(
                toolbar_wrapper, ViewCompat.getTransitionName(toolbar_wrapper)!!
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

// Temporary adapter
private class SessionsAdapter(val context: Context) : RecyclerView.Adapter<SessionsAdapter.ViewHolder>() {
    class ViewHolder(val textView: TextView) : RecyclerView.ViewHolder(textView) {}

    @RequiresApi(Build.VERSION_CODES.M)
    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): ViewHolder {
        val textView = TextView(context).apply {
            val lp = RelativeLayout.LayoutParams(
                RelativeLayout.LayoutParams.WRAP_CONTENT, // Width of TextView
                RelativeLayout.LayoutParams.WRAP_CONTENT
            )
            setLayoutParams(lp)
            setText("This is a sample TextView...")
            setTextColor(Color.parseColor("#ff0000"))
        }
        return ViewHolder(textView)
    }

    override fun getItemCount(): Int = 100

    override fun onBindViewHolder(holder: ViewHolder, position: Int) {
        holder.textView.text = "Cell: ${position}"
    }
}