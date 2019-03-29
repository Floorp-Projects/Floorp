/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.search

import androidx.constraintlayout.widget.ConstraintLayout
import androidx.constraintlayout.widget.ConstraintLayout.LayoutParams.PARENT_ID
import androidx.constraintlayout.widget.ConstraintLayout.LayoutParams.UNSET
import androidx.recyclerview.widget.LinearLayoutManager
import kotlinx.android.synthetic.main.component_awesomebar.*
import kotlinx.android.synthetic.main.fragment_search.*
import mozilla.components.support.base.log.logger.Logger
import org.jetbrains.anko.constraint.layout.ConstraintSetBuilder.Side.BOTTOM
import org.jetbrains.anko.constraint.layout.ConstraintSetBuilder.Side.TOP
import org.jetbrains.anko.constraint.layout.applyConstraintSet
import org.mozilla.fenix.Experiments.AATestDescriptor
import org.mozilla.fenix.isInExperiment

internal fun SearchFragment.layoutComponents(layout: ConstraintLayout) {
    context?.let {
        when {
            it.isInExperiment(AATestDescriptor) -> {
                setInExperimentConstraints(layout)
            }
            else -> {
                setOutOfExperimentConstraints(layout)
            }
        }
    } // we're unattached if context is null
}

internal fun SearchFragment.setInExperimentConstraints(layout: ConstraintLayout) {
    Logger.debug("Loading in experiment constraints")
    layout.applyConstraintSet {
        toolbar_wrapper {
            connect(
                TOP to TOP of UNSET,
                BOTTOM to TOP of pill_wrapper
            )
        }
        awesomeBar {
            connect(
                TOP to TOP of PARENT_ID,
                BOTTOM to TOP of toolbar_wrapper
            )
        }
        (awesomeBar.layoutManager as? LinearLayoutManager)?.reverseLayout = true
        pill_wrapper {
            connect(
                BOTTOM to BOTTOM of PARENT_ID
            )
        }
    }
}

internal fun SearchFragment.setOutOfExperimentConstraints(layout: ConstraintLayout) {
    Logger.debug("Loading out of experiment constraints")
    layout.applyConstraintSet {
        toolbar_wrapper {
            connect(
                TOP to TOP of PARENT_ID,
                BOTTOM to TOP of UNSET
            )
        }
        search_with_shortcuts {
            connect(
                TOP to BOTTOM of toolbar_wrapper
            )
        }
        awesomeBar {
            connect(
                TOP to TOP of UNSET,
                TOP to BOTTOM of search_with_shortcuts,
                BOTTOM to TOP of pill_wrapper
            )
        }
        (awesomeBar.layoutManager as? LinearLayoutManager)?.reverseLayout = false
        pill_wrapper {
            connect(
                BOTTOM to BOTTOM of PARENT_ID
            )
        }
    }
}
