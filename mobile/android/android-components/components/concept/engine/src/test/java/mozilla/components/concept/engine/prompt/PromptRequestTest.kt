/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.engine.prompt

import mozilla.components.concept.engine.prompt.PromptRequest.SingleChoice
import mozilla.components.concept.engine.prompt.PromptRequest.MultipleChoice
import mozilla.components.concept.engine.prompt.PromptRequest.MenuChoice
import mozilla.components.concept.engine.prompt.PromptRequest.Alert
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertEquals
import org.junit.Test

class PromptRequestTest {

    @Test
    fun `Create prompt requests`() {

        val single = SingleChoice(emptyArray()) {}
        single.onSelect(Choice(id = "", label = ""))
        assertNotNull(single.choices)

        val multiple = MultipleChoice(emptyArray()) {}
        multiple.onSelect(arrayOf(Choice(id = "", label = "")))
        assertNotNull(multiple.choices)

        val menu = MenuChoice(emptyArray()) {}
        menu.onSelect(Choice(id = "", label = ""))
        assertNotNull(menu.choices)

        val alert = Alert("title", "message", true, {}) {}
        assertEquals(alert.title, "title")
        assertEquals(alert.message, "message")
        assertEquals(alert.hasShownManyDialogs, true)
        alert.onDismiss()
        alert.onShouldShowNoMoreDialogs(true)
    }
}