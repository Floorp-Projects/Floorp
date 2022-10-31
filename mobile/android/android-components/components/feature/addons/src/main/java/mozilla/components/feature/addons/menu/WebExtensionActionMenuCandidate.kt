/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.addons.menu

import android.content.Context
import androidx.appcompat.content.res.AppCompatResources.getDrawable
import androidx.core.graphics.drawable.toDrawable
import mozilla.components.concept.engine.webextension.Action
import mozilla.components.concept.menu.candidate.AsyncDrawableMenuIcon
import mozilla.components.concept.menu.candidate.ContainerStyle
import mozilla.components.concept.menu.candidate.TextMenuCandidate
import mozilla.components.concept.menu.candidate.TextMenuIcon
import mozilla.components.concept.menu.candidate.TextStyle
import mozilla.components.feature.addons.R

/**
 * Create a browser menu item for displaying a web extension action.
 *
 * @param onClick a callback to be invoked when this menu item is clicked.
 */
fun Action.createMenuCandidate(
    context: Context,
    onClick: () -> Unit = this.onClick,
): TextMenuCandidate {
    return TextMenuCandidate(
        title.orEmpty(),
        start = loadIcon?.let { loadIcon ->
            val defaultIcon = getDrawable(context, R.drawable.mozac_ic_web_extension_default_icon)
            AsyncDrawableMenuIcon(
                loadDrawable = { _, height ->
                    loadIcon(height)?.toDrawable(context.resources)
                },
                loadingDrawable = defaultIcon,
                fallbackDrawable = defaultIcon,
            )
        },
        end = badgeText?.let { badgeText ->
            TextMenuIcon(
                badgeText,
                backgroundTint = badgeBackgroundColor,
                textStyle = TextStyle(
                    color = badgeTextColor,
                ),
            )
        },
        containerStyle = ContainerStyle(
            isVisible = true,
            isEnabled = enabled ?: false,
        ),
        onClick = onClick,
    )
}
