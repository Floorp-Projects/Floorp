/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.engine.webextension

import android.graphics.Color
import org.junit.Assert.assertEquals
import org.junit.Test

class ActionTest {

    private val onClick: () -> Unit = {}
    private val baseAction = Action(
        title = "title",
        enabled = false,
        loadIcon = null,
        badgeText = "badge",
        badgeTextColor = Color.BLACK,
        badgeBackgroundColor = Color.BLUE,
        onClick = onClick,
    )

    @Test
    fun `override using non-null attributes`() {
        val overridden = baseAction.copyWithOverride(
            Action(
                title = "other",
                enabled = null,
                loadIcon = null,
                badgeText = null,
                badgeTextColor = Color.WHITE,
                badgeBackgroundColor = null,
                onClick = onClick,
            ),
        )

        assertEquals(
            Action(
                title = "other",
                enabled = false,
                loadIcon = null,
                badgeText = "badge",
                badgeTextColor = Color.WHITE,
                badgeBackgroundColor = Color.BLUE,
                onClick = onClick,
            ),
            overridden,
        )
    }

    @Test
    fun `override using null action`() {
        val overridden = baseAction.copyWithOverride(null)

        assertEquals(baseAction, overridden)
    }
}
