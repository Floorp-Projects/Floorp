/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.search

import androidx.constraintlayout.widget.ConstraintLayout
import androidx.constraintlayout.widget.ConstraintLayout.LayoutParams.PARENT_ID
import androidx.constraintlayout.widget.ConstraintLayout.LayoutParams.TOP
import androidx.constraintlayout.widget.ConstraintLayout.LayoutParams.BOTTOM
import androidx.constraintlayout.widget.ConstraintSet
import androidx.recyclerview.widget.LinearLayoutManager
import kotlinx.android.synthetic.main.component_awesomebar.*
import kotlinx.android.synthetic.main.fragment_search.*
import mozilla.components.support.base.log.logger.Logger
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

    ConstraintSet().apply {
        clone(layout)

        // Move the search bar to the bottom of the layout
        clear(toolbar_wrapper.id, TOP)
        connect(toolbar_wrapper.id, BOTTOM, pill_wrapper.id, TOP)

        connect(awesomeBar.id, TOP, PARENT_ID, TOP)
        connect(awesomeBar.id, BOTTOM, toolbar_wrapper.id, TOP)
        (awesomeBar.layoutManager as? LinearLayoutManager)?.reverseLayout = true

        connect(pill_wrapper.id, BOTTOM, PARENT_ID, BOTTOM)

        applyTo(layout)
    }
}

internal fun SearchFragment.setOutOfExperimentConstraints(layout: ConstraintLayout) {
    Logger.debug("Loading out of experiment constraints")

    ConstraintSet().apply {
        clone(layout)

        // Move the search bar to the top of the layout
        connect(toolbar_wrapper.id, TOP, PARENT_ID, TOP)
        clear(toolbar_wrapper.id, BOTTOM)

        connect(fill_link_from_clipboard.id, TOP, toolbar_wrapper.id, BOTTOM)

        clear(awesomeBar.id, TOP)
        connect(awesomeBar.id, TOP, search_with_shortcuts.id, BOTTOM)
        connect(awesomeBar.id, BOTTOM, pill_wrapper.id, TOP)
        (awesomeBar.layoutManager as? LinearLayoutManager)?.reverseLayout = false

        connect(pill_wrapper.id, BOTTOM, PARENT_ID, BOTTOM)

        applyTo(layout)
    }
}
