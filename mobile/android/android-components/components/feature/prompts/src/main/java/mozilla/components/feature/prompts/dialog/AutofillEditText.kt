/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package mozilla.components.feature.prompts.dialog

import android.content.Context
import android.os.Build
import android.util.AttributeSet
import android.view.ViewStructure
import androidx.appcompat.widget.AppCompatEditText

/**
 * [androidx.appcompat.widget.AppCompatEditText] implementation to add WebDomain information which
 * allows autofill applications to detect which URL is requesting the authentication info.
 */
internal class AutofillEditText : AppCompatEditText {
    internal var url: String? = null

    constructor (context: Context) : super(context)

    constructor (context: Context, attrs: AttributeSet?) : super(context, attrs)

    constructor (context: Context, attrs: AttributeSet?, defStyleAttr: Int) : super(context, attrs, defStyleAttr)

    override fun onProvideAutofillStructure(structure: ViewStructure?, flags: Int) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O && url != null) {
            structure?.setWebDomain(url)
        }
        super.onProvideAutofillStructure(structure, flags)
    }
}
