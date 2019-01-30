package org.mozilla.fenix.home.sessions

import androidx.constraintlayout.widget.ConstraintLayout
import androidx.constraintlayout.widget.ConstraintLayout.LayoutParams.PARENT_ID
import kotlinx.android.synthetic.main.component_sessions.*
import kotlinx.android.synthetic.main.fragment_home.*
import org.jetbrains.anko.constraint.layout.ConstraintSetBuilder.Side.*
import org.jetbrains.anko.constraint.layout.applyConstraintSet
import org.mozilla.fenix.home.HomeFragment

fun HomeFragment.layoutComponents(layout: ConstraintLayout) {
    layout.applyConstraintSet {
        session_list {
            connect(
                BOTTOM to BOTTOM of PARENT_ID,
                START to START of PARENT_ID,
                END to END of PARENT_ID,
                TOP to BOTTOM of toolbar_wrapper
            )
        }
    }
}