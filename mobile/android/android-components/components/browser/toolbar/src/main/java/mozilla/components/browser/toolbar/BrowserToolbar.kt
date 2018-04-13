/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.toolbar

import android.content.Context
import android.util.AttributeSet
import android.view.inputmethod.EditorInfo
import android.widget.EditText
import android.widget.FrameLayout
import mozilla.components.concept.toolbar.Toolbar

/**
 * A customizable toolbar for browsers.
 */
class BrowserToolbar @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = 0
) : FrameLayout(context, attrs, defStyleAttr), Toolbar {

    override fun displayUrl(url: String) {
        val urlView = getUrlViewComponent()
        urlView.setText(url)
        urlView.setSelection(0, url.length)
    }

    override fun setOnUrlChangeListener(listener: (String) -> Unit) {
        val urlView = getUrlViewComponent()
        urlView.setOnEditorActionListener { _, actionId, _ ->
            if (actionId == EditorInfo.IME_ACTION_GO) {
                listener.invoke(urlView.text.toString())
            }
            true
        }
    }

    internal fun getUrlViewComponent(): EditText {
        return this.findViewById(R.id.urlView)
    }
}
