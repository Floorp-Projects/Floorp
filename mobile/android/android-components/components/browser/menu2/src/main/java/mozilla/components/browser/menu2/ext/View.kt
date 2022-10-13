/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.menu2.ext

import android.content.res.ColorStateList
import android.os.Build
import android.os.Build.VERSION.SDK_INT
import android.view.View
import android.widget.ImageView
import android.widget.TextView
import androidx.core.content.ContextCompat
import androidx.core.view.isVisible
import mozilla.components.concept.menu.candidate.ContainerStyle
import mozilla.components.concept.menu.candidate.HighPriorityHighlightEffect
import mozilla.components.concept.menu.candidate.LowPriorityHighlightEffect
import mozilla.components.concept.menu.candidate.MenuCandidateEffect
import mozilla.components.concept.menu.candidate.MenuEffect
import mozilla.components.concept.menu.candidate.MenuIconWithDrawable
import mozilla.components.concept.menu.candidate.TextStyle
import mozilla.components.support.ktx.android.content.res.resolveAttribute

/**
 * Apply container styles if different from the previous styling.
 */
internal fun View.applyStyle(newStyle: ContainerStyle, oldStyle: ContainerStyle?) {
    if (newStyle != oldStyle) {
        isVisible = newStyle.isVisible
        isEnabled = newStyle.isEnabled
    }
}

/**
 * Apply text styles if different from the previous styling.
 */
internal fun TextView.applyStyle(newStyle: TextStyle, oldStyle: TextStyle?) {
    if (newStyle != oldStyle) {
        newStyle.size?.let { textSize = it }
        newStyle.color?.let { setTextColor(it) }
        setTypeface(typeface, newStyle.textStyle)
        textAlignment = newStyle.textAlignment
    }
}

/**
 * Set the image to display based on the [MenuIconWithDrawable].
 */
internal fun ImageView.applyIcon(newIcon: MenuIconWithDrawable, oldIcon: MenuIconWithDrawable?) {
    if (newIcon != oldIcon) {
        setImageDrawable(newIcon.drawable)
        imageTintList = newIcon.tint?.let { ColorStateList.valueOf(it) }
    }
}

internal fun ImageView.applyNotificationEffect(
    newEffect: LowPriorityHighlightEffect?,
    oldEffect: MenuEffect?,
) {
    if (newEffect != oldEffect) {
        isVisible = newEffect != null
        imageTintList = newEffect?.notificationTint?.let { ColorStateList.valueOf(it) }
    }
}

/**
 * Build a drawable to be used for the background of a menu option.
 */
internal fun View.applyBackgroundEffect(
    newEffect: MenuCandidateEffect?,
    oldEffect: MenuCandidateEffect?,
) {
    if (newEffect == oldEffect) return

    val highlight = newEffect as? HighPriorityHighlightEffect
    val selectableBackgroundRes = context.theme
        .resolveAttribute(android.R.attr.selectableItemBackground)

    if (highlight != null) {
        val selectableBackground = ContextCompat.getDrawable(
            context,
            selectableBackgroundRes,
        )

        setBackgroundColor(highlight.backgroundTint)
        if (SDK_INT >= Build.VERSION_CODES.M) {
            foreground = selectableBackground
        }
    } else {
        setBackgroundResource(selectableBackgroundRes)
        if (SDK_INT >= Build.VERSION_CODES.M) {
            foreground = null
        }
    }
}
