/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.gecko.prompt

import mozilla.components.browser.engine.gecko.GeckoEngineSession
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.prompt.Choice
import mozilla.components.concept.engine.prompt.PromptRequest.Alert
import mozilla.components.concept.engine.prompt.PromptRequest
import mozilla.components.concept.engine.prompt.PromptRequest.MultipleChoice
import mozilla.components.concept.engine.prompt.PromptRequest.SingleChoice
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito
import org.mozilla.geckoview.GeckoRuntime
import org.mozilla.geckoview.GeckoSession
import org.mozilla.geckoview.GeckoSession.PromptDelegate.Choice.CHOICE_TYPE_MULTIPLE
import org.mozilla.geckoview.GeckoSession.PromptDelegate.Choice.CHOICE_TYPE_SINGLE
import org.robolectric.RobolectricTestRunner
import mozilla.components.support.ktx.kotlin.toDate
import org.junit.Assert.assertNotNull
import org.mozilla.geckoview.GeckoSession.PromptDelegate.TextCallback
import org.mozilla.geckoview.GeckoSession.PromptDelegate.DATETIME_TYPE_DATE
import org.mozilla.geckoview.GeckoSession.PromptDelegate.DATETIME_TYPE_DATETIME_LOCAL
import org.mozilla.geckoview.GeckoSession.PromptDelegate.DATETIME_TYPE_MONTH
import org.mozilla.geckoview.GeckoSession.PromptDelegate.DATETIME_TYPE_TIME
import org.mozilla.geckoview.GeckoSession.PromptDelegate.DATETIME_TYPE_WEEK
import java.util.Date
import java.util.Calendar
import java.util.Calendar.YEAR

typealias GeckoChoice = GeckoSession.PromptDelegate.Choice

@RunWith(RobolectricTestRunner::class)
class GeckoPromptDelegateTest {

    @Test
    fun `onChoicePrompt called with CHOICE_TYPE_SINGLE must provide a SingleChoice PromptRequest`() {
        val mockSession = GeckoEngineSession(Mockito.mock(GeckoRuntime::class.java))
        var promptRequestSingleChoice: PromptRequest = MultipleChoice(arrayOf()) {}
        var confirmWasCalled = false

        val callback = object : GeckoSession.PromptDelegate.ChoiceCallback {
            override fun confirm(id: String?) {}

            override fun confirm(items: Array<out GeckoSession.PromptDelegate.Choice>?) {}

            override fun dismiss() {}

            override fun getCheckboxValue() = false

            override fun setCheckboxValue(value: Boolean) {}

            override fun hasCheckbox() = false

            override fun getCheckboxMessage() = ""

            override fun confirm(ids: Array<out String>?) {}

            override fun confirm(item: GeckoChoice?) {
                confirmWasCalled = true
            }
        }

        val gecko = GeckoPromptDelegate(mockSession)
        val geckoChoices = arrayOf<GeckoChoice>()

        mockSession.register(object : EngineSession.Observer {
            override fun onPromptRequest(promptRequest: PromptRequest) {
                promptRequestSingleChoice = promptRequest
            }
        })

        gecko.onChoicePrompt(null, null, null, CHOICE_TYPE_SINGLE, geckoChoices, callback)

        assertTrue(promptRequestSingleChoice is SingleChoice)

        (promptRequestSingleChoice as SingleChoice).onSelect(mock())
        assertTrue(confirmWasCalled)
    }

    @Test
    fun `onChoicePrompt called with CHOICE_TYPE_MULTIPLE must provide a MultipleChoice PromptRequest`() {
        val mockSession = GeckoEngineSession(Mockito.mock(GeckoRuntime::class.java))
        var promptRequestSingleChoice: PromptRequest = SingleChoice(arrayOf()) {}
        var confirmWasCalled = false

        val callback = object : GeckoSession.PromptDelegate.ChoiceCallback {
            override fun confirm(id: String?) {}

            override fun confirm(items: Array<out GeckoSession.PromptDelegate.Choice>?) {}

            override fun dismiss() {}

            override fun getCheckboxValue() = false

            override fun setCheckboxValue(value: Boolean) {}

            override fun hasCheckbox() = false

            override fun getCheckboxMessage() = ""

            override fun confirm(ids: Array<out String>?) {
                confirmWasCalled = true
            }

            override fun confirm(item: GeckoChoice?) {}
        }

        val gecko = GeckoPromptDelegate(mockSession)
        val geckoChoices = arrayOf<GeckoChoice>()

        mockSession.register(object : EngineSession.Observer {
            override fun onPromptRequest(promptRequest: PromptRequest) {
                promptRequestSingleChoice = promptRequest
            }
        })

        gecko.onChoicePrompt(null, null, null, CHOICE_TYPE_MULTIPLE, geckoChoices, callback)

        assertTrue(promptRequestSingleChoice is MultipleChoice)

        (promptRequestSingleChoice as MultipleChoice).onSelect(arrayOf())
        assertTrue(confirmWasCalled)
    }

    @Test
    fun `onChoicePrompt called with CHOICE_TYPE_MENU must provide a MenuChoice PromptRequest`() {
        val mockSession = GeckoEngineSession(Mockito.mock(GeckoRuntime::class.java))
        var promptRequestSingleChoice: PromptRequest = PromptRequest.MenuChoice(arrayOf()) {}
        var confirmWasCalled = false

        val callback = object : GeckoSession.PromptDelegate.ChoiceCallback {
            override fun confirm(id: String?) = Unit
            override fun confirm(items: Array<out GeckoSession.PromptDelegate.Choice>?) = Unit
            override fun dismiss() = Unit
            override fun getCheckboxValue() = false
            override fun setCheckboxValue(value: Boolean) = Unit
            override fun hasCheckbox() = false
            override fun getCheckboxMessage() = ""
            override fun confirm(ids: Array<out String>?) = Unit

            override fun confirm(item: GeckoChoice?) {
                confirmWasCalled = true
            }
        }

        val gecko = GeckoPromptDelegate(mockSession)
        val geckoChoices = arrayOf<GeckoChoice>()

        mockSession.register(
                object : EngineSession.Observer {
                    override fun onPromptRequest(promptRequest: PromptRequest) {
                        promptRequestSingleChoice = promptRequest
                    }
                })

        gecko.onChoicePrompt(
                null, null, null,
                GeckoSession.PromptDelegate.Choice.CHOICE_TYPE_MENU, geckoChoices, callback
        )

        assertTrue(promptRequestSingleChoice is PromptRequest.MenuChoice)

        (promptRequestSingleChoice as PromptRequest.MenuChoice).onSelect(mock())
        assertTrue(confirmWasCalled)
    }

    @Test
    fun `onAlert must provide an alert PromptRequest`() {
        val mockSession = GeckoEngineSession(Mockito.mock(GeckoRuntime::class.java))
        var alertRequest: PromptRequest? = null
        var dismissWasCalled = false
        var setCheckboxValueWasCalled = false

        val callback = object : GeckoSession.PromptDelegate.AlertCallback {

            override fun setCheckboxValue(value: Boolean) {
                setCheckboxValueWasCalled = true
            }

            override fun dismiss() {
                dismissWasCalled = true
            }

            override fun getCheckboxValue(): Boolean = false
            override fun hasCheckbox(): Boolean = false
            override fun getCheckboxMessage(): String = ""
        }

        val promptDelegate = GeckoPromptDelegate(mockSession)

        mockSession.register(object : EngineSession.Observer {
            override fun onPromptRequest(promptRequest: PromptRequest) {
                alertRequest = promptRequest
            }
        })

        promptDelegate.onAlert(null, "title", "message", callback)

        assertTrue(alertRequest is PromptRequest.Alert)

        (alertRequest as Alert).onDismiss()
        assertTrue(dismissWasCalled)

        (alertRequest as Alert).onShouldShowNoMoreDialogs(true)
        assertTrue(setCheckboxValueWasCalled)

        assertEquals((alertRequest as Alert).title, "title")
        assertEquals((alertRequest as Alert).message, "message")
    }

    @Test
    fun `hitting functions not yet implemented`() {
        val mockSession = GeckoEngineSession(Mockito.mock(GeckoRuntime::class.java))
        val gecko = GeckoPromptDelegate(mockSession)
        gecko.onButtonPrompt(null, "", "", null, null)
        gecko.onDateTimePrompt(null, "", 0, null, null, null, mock())
        gecko.onFilePrompt(null, "", 0, null, null)
        gecko.onColorPrompt(null, "", "", null)
        gecko.onAuthPrompt(null, "", "", null, null)
        gecko.onTextPrompt(null, "", "", null, null)
        gecko.onPopupRequest(null, "")
        gecko.onDateTimePrompt(null, "", DATETIME_TYPE_TIME, null, "", "", mock())
        gecko.onDateTimePrompt(null, null, DATETIME_TYPE_DATETIME_LOCAL, "", "", "", mock())
        gecko.onDateTimePrompt(null, "", DATETIME_TYPE_DATETIME_LOCAL, "", "", "", mock())
    }

    @Test
    fun `hitting default values`() {
        val mockSession = GeckoEngineSession(Mockito.mock(GeckoRuntime::class.java))
        val gecko = GeckoPromptDelegate(mockSession)
        gecko.onDateTimePrompt(null, null, DATETIME_TYPE_DATE, null, null, null, mock())
        gecko.onDateTimePrompt(null, null, DATETIME_TYPE_WEEK, null, null, null, mock())
        gecko.onDateTimePrompt(null, null, DATETIME_TYPE_MONTH, null, null, null, mock())
        gecko.onDateTimePrompt(null, null, DATETIME_TYPE_TIME, null, "", "", mock())
    }

    @Test
    fun `toIdsArray must convert an list of choices to array of id strings`() {
        val choices = arrayOf(Choice(id = "0", label = ""), Choice(id = "1", label = ""))
        val ids = choices.toIdsArray()
        ids.forEachIndexed { index, item ->
            assertEquals("$index", item)
        }
    }

    @Test
    fun `onDateTimePrompt called with DATETIME_TYPE_DATE must provide a date PromptRequest`() {
        val mockSession = GeckoEngineSession(Mockito.mock(GeckoRuntime::class.java))
        var dateRequest: PromptRequest? = null
        var confirmCalled = false
        var onClearPicker = false

        val callback = object : TextCallback {
            override fun dismiss() = Unit
            override fun getCheckboxValue() = false
            override fun setCheckboxValue(value: Boolean) = Unit
            override fun hasCheckbox() = false
            override fun getCheckboxMessage() = ""
            override fun confirm(text: String?) {
                confirmCalled = true
                if (text!!.isEmpty()) {
                    onClearPicker = true
                }
            }
        }
        val promptDelegate = GeckoPromptDelegate(mockSession)
        mockSession.register(object : EngineSession.Observer {
            override fun onPromptRequest(promptRequest: PromptRequest) {
                dateRequest = promptRequest
            }
        })
        promptDelegate.onDateTimePrompt(null, "title", DATETIME_TYPE_DATE, "", "", "", callback)
        assertTrue(dateRequest is PromptRequest.Date)
        (dateRequest as PromptRequest.Date).onSelect(Date())
        assertTrue(confirmCalled)
        assertEquals((dateRequest as PromptRequest.Date).title, "title")

        (dateRequest as PromptRequest.Date).onClear()
        assertTrue(onClearPicker)
    }

    @Test
    fun `onDateTimePrompt DATETIME_TYPE_DATE with date parameters must format dates correctly`() {
        val mockSession = GeckoEngineSession(Mockito.mock(GeckoRuntime::class.java))
        var dateRequest: PromptRequest.Date? = null
        var geckoDate: String? = null
        val callback = object : TextCallback {
            override fun dismiss() = Unit
            override fun getCheckboxValue() = false
            override fun setCheckboxValue(value: Boolean) = Unit
            override fun hasCheckbox() = false
            override fun getCheckboxMessage() = ""
            override fun confirm(text: String?) {
                geckoDate = text
            }
        }
        val promptDelegate = GeckoPromptDelegate(mockSession)
        mockSession.register(object : EngineSession.Observer {
            override fun onPromptRequest(promptRequest: PromptRequest) {
                dateRequest = promptRequest as PromptRequest.Date
            }
        })
        promptDelegate.onDateTimePrompt(
                null,
                "title",
                DATETIME_TYPE_DATE,
                "2019-11-29",
                "2019-11-28",
                "2019-11-30",
                callback
        )
        assertNotNull(dateRequest)
        with(dateRequest!!) {
            assertEquals(initialDate, "2019-11-29".toDate("yyyy-MM-dd"))
            assertEquals(minimumDate, "2019-11-28".toDate("yyyy-MM-dd"))
            assertEquals(maximumDate, "2019-11-30".toDate("yyyy-MM-dd"))
        }
        val selectedDate = "2019-11-28".toDate("yyyy-MM-dd")
        (dateRequest as PromptRequest.Date).onSelect(selectedDate)
        assertNotNull(geckoDate?.toDate("yyyy-MM-dd")?.equals(selectedDate))
        assertEquals((dateRequest as PromptRequest.Date).title, "title")
    }

    @Test
    fun `onDateTimePrompt called with DATETIME_TYPE_MONTH must provide a date PromptRequest`() {
        val mockSession = GeckoEngineSession(Mockito.mock(GeckoRuntime::class.java))
        var dateRequest: PromptRequest? = null
        var confirmCalled = false
        val callback = object : TextCallback {
            override fun dismiss() = Unit
            override fun getCheckboxValue() = false
            override fun setCheckboxValue(value: Boolean) = Unit
            override fun hasCheckbox() = false
            override fun getCheckboxMessage() = ""
            override fun confirm(text: String?) {
                confirmCalled = true
            }
        }
        val promptDelegate = GeckoPromptDelegate(mockSession)
        mockSession.register(object : EngineSession.Observer {
            override fun onPromptRequest(promptRequest: PromptRequest) {
                dateRequest = promptRequest
            }
        })
        promptDelegate.onDateTimePrompt(null, "title", DATETIME_TYPE_MONTH, "", "", "", callback)
        assertTrue(dateRequest is PromptRequest.Date)
        (dateRequest as PromptRequest.Date).onSelect(Date())
        assertTrue(confirmCalled)
        assertEquals((dateRequest as PromptRequest.Date).title, "title")
    }

    @Test
    fun `onDateTimePrompt DATETIME_TYPE_MONTH with date parameters must format dates correctly`() {
        val mockSession = GeckoEngineSession(Mockito.mock(GeckoRuntime::class.java))
        var dateRequest: PromptRequest.Date? = null
        var geckoDate: String? = null
        val callback = object : TextCallback {
            override fun dismiss() = Unit
            override fun getCheckboxValue() = false
            override fun setCheckboxValue(value: Boolean) = Unit
            override fun hasCheckbox() = false
            override fun getCheckboxMessage() = ""
            override fun confirm(text: String?) {
                geckoDate = text
            }
        }
        val promptDelegate = GeckoPromptDelegate(mockSession)
        mockSession.register(object : EngineSession.Observer {
            override fun onPromptRequest(promptRequest: PromptRequest) {
                dateRequest = promptRequest as PromptRequest.Date
            }
        })
        promptDelegate.onDateTimePrompt(
                null,
                "title",
                DATETIME_TYPE_MONTH,
                "2019-11",
                "2019-11",
                "2019-11",
                callback
        )
        assertNotNull(dateRequest)
        with(dateRequest!!) {
            assertEquals(initialDate, "2019-11".toDate("yyyy-MM"))
            assertEquals(minimumDate, "2019-11".toDate("yyyy-MM"))
            assertEquals(maximumDate, "2019-11".toDate("yyyy-MM"))
        }
        val selectedDate = "2019-11".toDate("yyyy-MM")
        (dateRequest as PromptRequest.Date).onSelect(selectedDate)
        assertNotNull(geckoDate?.toDate("yyyy-MM")?.equals(selectedDate))
        assertEquals((dateRequest as PromptRequest.Date).title, "title")
    }

    @Test
    fun `onDateTimePrompt called with DATETIME_TYPE_WEEK must provide a date PromptRequest`() {
        val mockSession = GeckoEngineSession(Mockito.mock(GeckoRuntime::class.java))
        var dateRequest: PromptRequest? = null
        var confirmCalled = false
        val callback = object : TextCallback {
            override fun dismiss() = Unit
            override fun getCheckboxValue() = false
            override fun setCheckboxValue(value: Boolean) = Unit
            override fun hasCheckbox() = false
            override fun getCheckboxMessage() = ""
            override fun confirm(text: String?) {
                confirmCalled = true
            }
        }
        val promptDelegate = GeckoPromptDelegate(mockSession)
        mockSession.register(object : EngineSession.Observer {
            override fun onPromptRequest(promptRequest: PromptRequest) {
                dateRequest = promptRequest
            }
        })
        promptDelegate.onDateTimePrompt(null, "title", DATETIME_TYPE_WEEK, "", "", "", callback)
        assertTrue(dateRequest is PromptRequest.Date)
        (dateRequest as PromptRequest.Date).onSelect(Date())
        assertTrue(confirmCalled)
        assertEquals((dateRequest as PromptRequest.Date).title, "title")
    }

    @Test
    fun `onDateTimePrompt DATETIME_TYPE_WEEK with date parameters must format dates correctly`() {
        val mockSession = GeckoEngineSession(Mockito.mock(GeckoRuntime::class.java))
        var dateRequest: PromptRequest.Date? = null
        var geckoDate: String? = null
        val callback = object : TextCallback {
            override fun dismiss() = Unit
            override fun getCheckboxValue() = false
            override fun setCheckboxValue(value: Boolean) = Unit
            override fun hasCheckbox() = false
            override fun getCheckboxMessage() = ""
            override fun confirm(text: String?) {
                geckoDate = text
            }
        }
        val promptDelegate = GeckoPromptDelegate(mockSession)
        mockSession.register(object : EngineSession.Observer {
            override fun onPromptRequest(promptRequest: PromptRequest) {
                dateRequest = promptRequest as PromptRequest.Date
            }
        })
        promptDelegate.onDateTimePrompt(
                null,
                "title",
                DATETIME_TYPE_WEEK,
                "2018-W18",
                "2018-W18",
                "2018-W26",
                callback
        )
        assertNotNull(dateRequest)
        with(dateRequest!!) {
            assertEquals(initialDate, "2018-W18".toDate("yyyy-'W'ww"))
            assertEquals(minimumDate, "2018-W18".toDate("yyyy-'W'ww"))
            assertEquals(maximumDate, "2018-W26".toDate("yyyy-'W'ww"))
        }
        val selectedDate = "2018-W26".toDate("yyyy-'W'ww")
        (dateRequest as PromptRequest.Date).onSelect(selectedDate)
        assertNotNull(geckoDate?.toDate("yyyy-'W'ww")?.equals(selectedDate))
        assertEquals((dateRequest as PromptRequest.Date).title, "title")
    }

    @Test
    fun `date to string`() {
        val date = Date()

        var dateString = date.toString()
        assertNotNull(dateString.isEmpty())

        dateString = date.toString("yyyy")
        val calendar = Calendar.getInstance()
        calendar.time = date
        val year = calendar[YEAR].toString()
        assertEquals(dateString, year)
    }
}