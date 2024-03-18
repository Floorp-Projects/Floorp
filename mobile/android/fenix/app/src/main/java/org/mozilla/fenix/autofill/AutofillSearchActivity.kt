/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.autofill

import android.os.Build
import android.os.Bundle
import android.view.ViewGroup
import androidx.annotation.RequiresApi
import mozilla.components.feature.autofill.AutofillConfiguration
import mozilla.components.feature.autofill.ui.AbstractAutofillSearchActivity
import org.mozilla.fenix.ext.components

/**
 * Activity responsible for letting the user manually search and pick credentials for auto-filling a
 * third-party app.
 */
@RequiresApi(Build.VERSION_CODES.O)
class AutofillSearchActivity : AbstractAutofillSearchActivity() {
    override val configuration: AutofillConfiguration by lazy { components.autofillConfiguration }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        // To avoid the dialog constantly resizing horizontally while typing, let's always use
        // the full width of the screen for the dialog.
        window.setLayout(
            ViewGroup.LayoutParams.MATCH_PARENT,
            ViewGroup.LayoutParams.WRAP_CONTENT,
        )
    }
}
