/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.engine.prompt

import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Test

class ChoiceTest {

    @Test
    fun `Create a choice`() {
        val choice = Choice(id = "id", label = "label", children = arrayOf())
        choice.selected = true
        choice.enable = true
        choice.label = "label"

        assertEquals(choice.id, "id")
        assertEquals(choice.label, "label")
        assertEquals(choice.describeContents(), 0)
        assertTrue(choice.enable)
        assertFalse(choice.isASeparator)
        assertTrue(choice.selected)
        assertTrue(choice.isGroupType)
        assertNotNull(choice.children)

        choice.writeToParcel(mock(), 0)
        Choice(mock())
        Choice.createFromParcel(mock())
        Choice.newArray(1)
    }
}
