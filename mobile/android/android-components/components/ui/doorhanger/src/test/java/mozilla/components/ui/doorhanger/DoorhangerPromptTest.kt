/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:Suppress("DEPRECATION")

package mozilla.components.ui.doorhanger

import android.view.View
import android.view.ViewGroup
import android.widget.Button
import android.widget.CheckBox
import android.widget.LinearLayout
import android.widget.RadioButton
import android.widget.RadioGroup
import android.widget.TextView
import androidx.appcompat.content.res.AppCompatResources
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class DoorhangerPromptTest {

    @Test
    fun `Create complex prompt doorhanger`() {
        var allowButtonClicked = false
        var dontAllowButtonClicked = false

        val prompt = DoorhangerPrompt(
            title = "Allow wikipedia.org to use your camera and microphone?",
            controlGroups = listOf(
                DoorhangerPrompt.ControlGroup(
                    icon = AppCompatResources.getDrawable(testContext, android.R.drawable.ic_menu_camera),
                    controls = listOf(
                        DoorhangerPrompt.Control.RadioButton("Front-facing camera"),
                        DoorhangerPrompt.Control.RadioButton("Selfie camera")
                    )
                ),
                DoorhangerPrompt.ControlGroup(
                    icon = AppCompatResources.getDrawable(testContext, android.R.drawable.ic_btn_speak_now),
                    controls = listOf(
                        DoorhangerPrompt.Control.RadioButton("Speakerphone"),
                        DoorhangerPrompt.Control.RadioButton("Headset microphone"),
                        DoorhangerPrompt.Control.CheckBox(
                            "Don't ask again on this site",
                            checked = true
                        )
                    )
                )
            ),
            buttons = listOf(
                DoorhangerPrompt.Button("Don't allow") {
                    dontAllowButtonClicked = true
                },
                DoorhangerPrompt.Button("Allow", positive = true) {
                    allowButtonClicked = true
                }
            ),
            onDismiss = {}
        )

        val doorhanger = prompt.createDoorhanger(testContext)
        assertNotNull(doorhanger)

        val popupWindow = doorhanger.show(View(testContext))
        assertNotNull(popupWindow)

        val contentView = popupWindow.contentView
        assertNotNull(contentView)

        // --------------------------------------------------------------------------------------------
        // Title
        // --------------------------------------------------------------------------------------------

        val titleView = contentView.findViewById<TextView>(R.id.title)
        assertNotNull(titleView)
        assertEquals("Allow wikipedia.org to use your camera and microphone?", titleView.text)

        // --------------------------------------------------------------------------------------------
        // Control groups:
        // --------------------------------------------------------------------------------------------

        val controlsContainer = contentView.findViewById<ViewGroup>(R.id.controls)
        assertNotNull(controlsContainer)
        assertEquals(3, controlsContainer.childCount) // 2 groups + 1 divider

        // --------------------------------------------------------------------------------------------
        // First control group:
        // --------------------------------------------------------------------------------------------

        val group1 = controlsContainer.getChildAt(0).findViewById<ViewGroup>(R.id.group)
        assertEquals(2, group1.childCount)

        val radioButton11 = group1.getChildAt(0) as RadioButton
        assertEquals("Front-facing camera", radioButton11.text)
        assertTrue(radioButton11.isChecked)

        val radioButton12 = group1.getChildAt(1) as RadioButton
        assertEquals("Selfie camera", radioButton12.text)
        assertFalse(radioButton12.isChecked)

        // --------------------------------------------------------------------------------------------
        // Second control group:
        // --------------------------------------------------------------------------------------------

        val group2 = controlsContainer.getChildAt(2).findViewById<ViewGroup>(R.id.group)
        assertEquals(3, group2.childCount)

        val radioButton21 = group2.getChildAt(0) as RadioButton
        assertEquals("Speakerphone", radioButton21.text)
        assertTrue(radioButton21.isChecked)

        val radioButton22 = group2.getChildAt(1) as RadioButton
        assertEquals("Headset microphone", radioButton22.text)
        assertFalse(radioButton22.isChecked)

        val checkBox23 = group2.getChildAt(2) as CheckBox
        assertEquals("Don't ask again on this site", checkBox23.text)
        assertTrue(checkBox23.isChecked)

        // --------------------------------------------------------------------------------------------
        // Buttons
        // --------------------------------------------------------------------------------------------

        val buttonsContainer = contentView.findViewById<ViewGroup>(R.id.buttons)
        assertNotNull(buttonsContainer)
        assertEquals(2, buttonsContainer.childCount)

        val button1 = buttonsContainer.getChildAt(0) as Button
        assertEquals("Don't allow", button1.text)

        val button2 = buttonsContainer.getChildAt(1) as Button
        assertEquals("Allow", button2.text)

        assertFalse(dontAllowButtonClicked)
        button1.performClick()
        assertTrue(dontAllowButtonClicked)

        assertFalse(allowButtonClicked)
        button2.performClick()
        assertTrue(allowButtonClicked)
    }

    @Test
    fun `RadioButton view updates data class`() {
        val button1 = DoorhangerPrompt.Control.RadioButton("Front-facing camera")
        val button2 = DoorhangerPrompt.Control.RadioButton("Back-facing camera")
        val button3 = DoorhangerPrompt.Control.RadioButton("Selfie camera")

        val controlGroup = DoorhangerPrompt.ControlGroup(
            icon = AppCompatResources.getDrawable(testContext, android.R.drawable.ic_menu_camera),
            controls = listOf(button1, button2, button3))

        val view = controlGroup.createView(testContext, LinearLayout(testContext))
        val group = view.findViewById<RadioGroup>(R.id.group)

        val radioButton1 = group.getChildAt(0) as RadioButton
        val radioButton2 = group.getChildAt(1) as RadioButton
        val radioButton3 = group.getChildAt(2) as RadioButton

        radioButton2.toggle()

        assertFalse(button1.checked)
        assertTrue(button2.checked)
        assertFalse(button3.checked)

        radioButton3.toggle()

        assertFalse(button1.checked)
        assertFalse(button2.checked)
        assertTrue(button3.checked)

        radioButton1.toggle()
        radioButton1.toggle()

        assertTrue(button1.checked)
        assertFalse(button2.checked)
        assertFalse(button3.checked)
    }

    @Test
    fun `CheckBox view updates data class`() {
        val checkbox1 = DoorhangerPrompt.Control.CheckBox("Okay?", checked = false)
        val checkbox2 = DoorhangerPrompt.Control.CheckBox("Ready?", checked = true)

        val controlGroup = DoorhangerPrompt.ControlGroup(
            controls = listOf(checkbox1, checkbox2))

        val view = controlGroup.createView(testContext, LinearLayout(testContext))
        val group = view.findViewById<RadioGroup>(R.id.group)

        val checkBoxView1 = group.getChildAt(0) as CheckBox
        val checkBoxView2 = group.getChildAt(1) as CheckBox

        assertFalse(checkBoxView1.isChecked)
        assertTrue(checkBoxView2.isChecked)

        checkBoxView1.toggle()

        assertTrue(checkbox1.checked)
        assertTrue(checkbox2.checked)

        checkBoxView2.toggle()

        assertTrue(checkbox1.checked)
        assertFalse(checkbox2.checked)
    }

    @Test
    fun `ControlGroup selects first radio button when creating view `() {
        val button1 = DoorhangerPrompt.Control.RadioButton("Front-facing camera")
        val button2 = DoorhangerPrompt.Control.RadioButton("Back-facing camera")
        val button3 = DoorhangerPrompt.Control.RadioButton("Selfie camera")

        assertFalse(button1.checked)
        assertFalse(button2.checked)
        assertFalse(button3.checked)

        val controlGroup = DoorhangerPrompt.ControlGroup(
            icon = AppCompatResources.getDrawable(testContext, android.R.drawable.ic_menu_camera),
            controls = listOf(button1, button2, button3))

        controlGroup.createView(testContext, LinearLayout(testContext))

        assertTrue(button1.checked)
        assertFalse(button2.checked)
        assertFalse(button3.checked)
    }

    @Test
    fun `Button has label assigned`() {
        val button = DoorhangerPrompt.Button("Hello World", positive = true, onClick = {})

        val view = button.createView(testContext, LinearLayout(testContext), onClick = {})

        assertTrue(view is Button)
        val buttonView = view as Button

        assertEquals("Hello World", buttonView.text)
    }

    @Test
    fun `Clicking Button invokes callback`() {
        val button = DoorhangerPrompt.Button("Hello World", positive = true, onClick = {})

        var buttonClicked = false

        val view = button.createView(testContext, LinearLayout(testContext), onClick = {
            buttonClicked = true
        })

        assertFalse(buttonClicked)

        view.performClick()

        assertTrue(buttonClicked)
    }
}
