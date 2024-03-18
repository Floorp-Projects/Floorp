/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.samples.browser.autofill

import android.os.Build
import androidx.annotation.RequiresApi
import mozilla.components.feature.autofill.AutofillConfiguration
import mozilla.components.feature.autofill.ui.AbstractAutofillConfirmActivity
import org.mozilla.samples.browser.ext.components

/**
 * Activity responsible for asking the user to confirm before autofilling a third-party app.
 */
@RequiresApi(Build.VERSION_CODES.O)
class AutofillConfirmActivity : AbstractAutofillConfirmActivity() {
    override val configuration: AutofillConfiguration by lazy { components.autofillConfiguration }
}
