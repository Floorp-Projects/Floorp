/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.toolbar

import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.utils.Settings

/**
 * An abstraction for the Toolbar Redesign feature.
 */
interface RedesignToolbarFeature {

    /**
     * Returns true if the toolbar redesign feature is enabled.
     */
    val isEnabled: Boolean
}

/**
 * The complete portions of the redesigned Toolbar ready for Nightly.
 *
 */
class CompleteRedesignToolbarFeature(
    private val settings: Settings,
) : RedesignToolbarFeature {

    override val isEnabled: Boolean
        get() = settings.enableRedesignToolbar
}

/**
 * The incomplete portions of the redesigned Toolbar still in progress.
 *
 */
class IncompleteRedesignToolbarFeature(
    private val settings: Settings,
) : RedesignToolbarFeature {

    override val isEnabled: Boolean
        get() = settings.enableIncompleteToolbarRedesign
}
