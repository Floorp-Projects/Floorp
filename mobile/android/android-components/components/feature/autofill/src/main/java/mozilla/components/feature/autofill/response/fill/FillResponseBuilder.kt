/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.autofill.response.fill

import android.content.Context
import android.service.autofill.FillResponse
import android.widget.inline.InlinePresentationSpec
import mozilla.components.feature.autofill.AutofillConfiguration

internal interface FillResponseBuilder {
    fun build(
        context: Context,
        configuration: AutofillConfiguration,
        imeSpec: InlinePresentationSpec? = null,
    ): FillResponse
}
