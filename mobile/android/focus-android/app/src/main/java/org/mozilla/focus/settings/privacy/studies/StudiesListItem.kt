/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.settings.privacy.studies

import androidx.annotation.StringRes
import org.mozilla.experiments.nimbus.EnrolledExperiment

sealed class StudiesListItem {
    data class Section(
        @StringRes val titleId: Int,
        val visibleDivider: Boolean = true,
    ) : StudiesListItem()

    data class ActiveStudy(val value: EnrolledExperiment) : StudiesListItem()
}
