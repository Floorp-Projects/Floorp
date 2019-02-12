/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home

import android.view.View
import androidx.constraintlayout.widget.ConstraintLayout.LayoutParams.PARENT_ID
import kotlinx.android.synthetic.main.component_sessions.view.*
import kotlinx.android.synthetic.main.component_tabs.view.*
import kotlinx.android.synthetic.main.fragment_home.view.*
import kotlinx.android.synthetic.main.tab_list_header.view.*
import org.jetbrains.anko.constraint.layout.ConstraintSetBuilder.Side.BOTTOM
import org.jetbrains.anko.constraint.layout.ConstraintSetBuilder.Side.END
import org.jetbrains.anko.constraint.layout.ConstraintSetBuilder.Side.START
import org.jetbrains.anko.constraint.layout.ConstraintSetBuilder.Side.TOP
import org.jetbrains.anko.constraint.layout.applyConstraintSet

fun HomeFragment.layoutComponents(layout: View) {
    with(layout) {
        homeLayout.applyConstraintSet {
            tabs_header {
                connect(
                    TOP to BOTTOM of homeDivider,
                    START to START of tabs_list,
                    END to END of PARENT_ID
                )
            }
            tabs_list {
                connect(
                    TOP to BOTTOM of tabs_header,
                    START to START of PARENT_ID,
                    END to END of PARENT_ID
                )
            }
            session_list {
                connect(
                    TOP to BOTTOM of tabs_list,
                    START to START of PARENT_ID,
                    END to END of PARENT_ID
                )
            }
        }
    }
}
