/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.compose.ext

import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.width
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import org.junit.Assert.assertEquals
import org.junit.Test

class ModifierTest {
    @Test
    fun `GIVEN predicate is false WHEN thenConditional called THEN the original modifier is returned`() {
        val modifier = Modifier.height(1.dp)

        assertEquals(modifier, modifier.thenConditional(Modifier.width(1.dp)) { false })
    }

    @Test
    fun `GIVEN predicate is true WHEN thenConditional called THEN the updated modifier is returned`() {
        val modifier = Modifier.height(1.dp)

        val expected = Modifier
            .height(1.dp)
            .width(1.dp)
        val actual = modifier.thenConditional(Modifier.width(1.dp)) { true }

        assertEquals(expected, actual)
    }
}
