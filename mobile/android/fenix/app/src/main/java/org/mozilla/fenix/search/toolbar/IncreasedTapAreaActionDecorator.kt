/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.search.toolbar

import android.view.View
import androidx.annotation.VisibleForTesting
import mozilla.components.concept.toolbar.Toolbar
import org.mozilla.fenix.ext.increaseTapArea

/**
 * A Decorator that accepts a [Toolbar.Action] and increases its tap area.
 */
class IncreasedTapAreaActionDecorator(
    private val action: Toolbar.Action,
) : Toolbar.Action by action {

    override fun bind(view: View) {
        action.bind(view)
        view.increaseTapArea(TAP_INCREASE_DPS)
    }

    companion object {
        @VisibleForTesting
        const val TAP_INCREASE_DPS = 8
    }
}
