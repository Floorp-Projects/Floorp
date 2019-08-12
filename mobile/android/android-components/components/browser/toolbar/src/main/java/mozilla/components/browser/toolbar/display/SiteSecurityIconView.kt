/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.toolbar.display

import android.content.Context
import android.util.AttributeSet
import android.view.View
import androidx.appcompat.widget.AppCompatImageView
import mozilla.components.browser.toolbar.R
import mozilla.components.concept.toolbar.Toolbar.SiteSecurity

/**
 * Internal widget to display the different icons of site security, relies on the
 * [SiteSecurity] state of each page.
 */
internal class SiteSecurityIconView @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = 0
) : AppCompatImageView(context, attrs, defStyleAttr) {

    // We allow null here because in some situations, onCreateDrawableState is getting called from
    // the super() constructor on the View class, way before this class properties get
    // initialized causing a null pointer exception.
    // See for more details: https://github.com/mozilla-mobile/android-components/issues/4058
    var siteSecurity: SiteSecurity? = SiteSecurity.INSECURE
        set(value) {
            if (value != field) {
                field = value
                refreshDrawableState()
            }

            field = value
        }

    override fun onCreateDrawableState(extraSpace: Int): IntArray {
        return when (siteSecurity) {
            SiteSecurity.INSECURE, null -> super.onCreateDrawableState(extraSpace)
            SiteSecurity.SECURE -> {
                val drawableState = super.onCreateDrawableState(extraSpace + 1)
                View.mergeDrawableStates(drawableState, intArrayOf(R.attr.state_site_secure))
                drawableState
            }
        }
    }
}
