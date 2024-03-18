/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.utils

import android.content.Context
import android.util.AttributeSet
import android.view.accessibility.AccessibilityNodeInfo
import android.view.accessibility.AccessibilityNodeInfo.AccessibilityAction.ACTION_LONG_CLICK
import androidx.appcompat.widget.AppCompatTextView
import org.mozilla.fenix.R

/**
 * An [AppCompatTextView] that announces as link in screen readers for a11y purposes
 */
class LinkTextView @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = 0,
) : AppCompatTextView(context, attrs, defStyleAttr) {

    override fun onInitializeAccessibilityNodeInfo(info: AccessibilityNodeInfo?) {
        super.onInitializeAccessibilityNodeInfo(info)
        val extras = info?.extras
        extras?.putCharSequence(
            "AccessibilityNodeInfo.roleDescription",
            context.resources.getString(R.string.link_text_view_type_announcement),
        )
        // disable long click  announcement, as there is no action to be performed on long click
        info?.isLongClickable = false
        info?.removeAction(ACTION_LONG_CLICK)
    }
}
