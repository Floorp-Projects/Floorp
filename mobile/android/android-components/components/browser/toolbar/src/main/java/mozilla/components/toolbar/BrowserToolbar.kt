/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.toolbar

import android.content.Context
import android.util.AttributeSet
import android.widget.FrameLayout
import android.widget.TextView

/**
 * A customizable toolbar for browsers.
 */
class BrowserToolbar @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = 0
) : FrameLayout(context, attrs, defStyleAttr), Toolbar {
    private val urlView = TextView(context)

    init {
        addView(urlView)
    }

    override fun displayUrl(url: String) {
        urlView.text = url
    }
}
