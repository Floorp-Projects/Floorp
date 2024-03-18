/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.autofill

import android.os.Build
import androidx.annotation.RequiresApi
import mozilla.components.feature.autofill.AutofillConfiguration
import mozilla.components.feature.autofill.ui.AbstractAutofillUnlockActivity
import org.mozilla.fenix.ext.components

/**
 * Activity responsible for unlocking the autofill service by asking the user to verify.
 */
@RequiresApi(Build.VERSION_CODES.O)
class AutofillUnlockActivity : AbstractAutofillUnlockActivity() {
    override val configuration: AutofillConfiguration by lazy { components.autofillConfiguration }
}
