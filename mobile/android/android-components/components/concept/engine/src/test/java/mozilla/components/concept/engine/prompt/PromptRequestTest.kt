/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.engine.prompt

import mozilla.components.concept.engine.prompt.PromptRequest.Alert
import mozilla.components.concept.engine.prompt.PromptRequest.MenuChoice
import mozilla.components.concept.engine.prompt.PromptRequest.MultipleChoice
import mozilla.components.concept.engine.prompt.PromptRequest.SingleChoice
import mozilla.components.concept.engine.prompt.PromptRequest.TimeSelection.Type
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Test
import java.util.Date

class PromptRequestTest {

    @Test
    fun `Create prompt requests`() {

        val single = SingleChoice(emptyArray()) {}
        single.onConfirm(Choice(id = "", label = ""))
        assertNotNull(single.choices)

        val multiple = MultipleChoice(emptyArray()) {}
        multiple.onConfirm(arrayOf(Choice(id = "", label = "")))
        assertNotNull(multiple.choices)

        val menu = MenuChoice(emptyArray()) {}
        menu.onConfirm(Choice(id = "", label = ""))
        assertNotNull(menu.choices)

        val alert = Alert("title", "message", true, {}) {}
        assertEquals(alert.title, "title")
        assertEquals(alert.message, "message")
        assertEquals(alert.hasShownManyDialogs, true)
        alert.onDismiss()
        alert.onConfirm(true)

        assertEquals(alert.title, "title")
        assertEquals(alert.message, "message")
        assertEquals(alert.hasShownManyDialogs, true)
        alert.onDismiss()
        alert.onConfirm(true)

        val textPrompt = PromptRequest.TextPrompt(
            "title",
            "label",
            "value",
            true,
            {}) { _, _ -> }

        assertEquals(textPrompt.title, "title")
        assertEquals(textPrompt.inputLabel, "label")
        assertEquals(textPrompt.inputValue, "value")
        assertEquals(textPrompt.hasShownManyDialogs, true)
        textPrompt.onDismiss()
        textPrompt.onConfirm(true, "")

        val dateRequest = PromptRequest.TimeSelection(
            "title",
            Date(),
            Date(),
            Date(),
            Type.DATE,
            {}) {}
        assertEquals(dateRequest.title, "title")
        assertEquals(dateRequest.type, Type.DATE)
        assertNotNull(dateRequest.initialDate)
        assertNotNull(dateRequest.minimumDate)
        assertNotNull(dateRequest.maximumDate)
        dateRequest.onConfirm(Date())
        dateRequest.onClear()

        val filePickerRequest = PromptRequest.File(
            emptyArray(),
            true,
            PromptRequest.File.FacingMode.NONE,
            { _, _ -> },
            { _, _ -> }
        ) {}
        assertTrue(filePickerRequest.mimeTypes.isEmpty())
        assertTrue(filePickerRequest.isMultipleFilesSelection)
        assertEquals(filePickerRequest.captureMode, PromptRequest.File.FacingMode.NONE)
        filePickerRequest.onSingleFileSelected(mock(), mock())
        filePickerRequest.onMultipleFilesSelected(mock(), emptyArray())
        filePickerRequest.onDismiss()

        val promptRequest = PromptRequest.Authentication(
            "title",
            "message",
            "username",
            "password",
            PromptRequest.Authentication.Method.HOST,
            PromptRequest.Authentication.Level.NONE,
            false,
            false,
            false,
            { _, _ -> }) {
        }

        assertEquals(promptRequest.title, "title")
        assertEquals(promptRequest.message, "message")
        assertEquals(promptRequest.userName, "username")
        assertEquals(promptRequest.password, "password")
        assertFalse(promptRequest.onlyShowPassword)
        assertFalse(promptRequest.previousFailed)
        assertFalse(promptRequest.isCrossOrigin)
        promptRequest.onConfirm("", "")
        promptRequest.onDismiss()

        val onConfirm: (String) -> Unit = {
        }
        val onDismiss: () -> Unit = {
        }

        val colorRequest = PromptRequest.Color("defaultColor", onConfirm, onDismiss)
        assertEquals(colorRequest.defaultColor, "defaultColor")

        colorRequest.onConfirm("")
        colorRequest.onDismiss()

        val popupRequest = PromptRequest.Popup("http://mozilla.slack.com/", {}, {})

        assertEquals(popupRequest.targetUri, "http://mozilla.slack.com/")
        popupRequest.onAllow()
        popupRequest.onDeny()

        val onConfirmPositiveButton: (Boolean) -> Unit = {
        }

        val onConfirmNegativeButton: (Boolean) -> Unit = {
        }

        val onConfirmNeutralButton: (Boolean) -> Unit = {
        }

        val confirmRequest = PromptRequest.Confirm(
            "title",
            "message",
            false,
            "positive",
            "negative",
            "neutral",
            onConfirmPositiveButton,
            onConfirmNegativeButton,
            onConfirmNeutralButton
        ) {}

        assertEquals(confirmRequest.title, "title")
        assertEquals(confirmRequest.message, "message")
        assertEquals(confirmRequest.positiveButtonTitle, "positive")
        assertEquals(confirmRequest.negativeButtonTitle, "negative")
        assertEquals(confirmRequest.neutralButtonTitle, "neutral")
        confirmRequest.onConfirmPositiveButton(true)
        confirmRequest.onConfirmNegativeButton(true)
        confirmRequest.onConfirmNeutralButton(true)
    }
}
