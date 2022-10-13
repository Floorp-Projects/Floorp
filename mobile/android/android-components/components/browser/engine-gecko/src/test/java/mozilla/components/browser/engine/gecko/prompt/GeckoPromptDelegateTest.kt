/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.gecko.prompt

import android.net.Uri
import android.os.Looper.getMainLooper
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.engine.gecko.GeckoEngineSession
import mozilla.components.browser.engine.gecko.ext.toAutocompleteAddress
import mozilla.components.browser.engine.gecko.ext.toAutocompleteCreditCard
import mozilla.components.browser.engine.gecko.ext.toLoginEntry
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.prompt.Choice
import mozilla.components.concept.engine.prompt.PromptRequest
import mozilla.components.concept.engine.prompt.PromptRequest.MultipleChoice
import mozilla.components.concept.engine.prompt.PromptRequest.SingleChoice
import mozilla.components.concept.storage.Address
import mozilla.components.concept.storage.CreditCardEntry
import mozilla.components.concept.storage.Login
import mozilla.components.concept.storage.LoginEntry
import mozilla.components.support.ktx.kotlin.toDate
import mozilla.components.support.test.any
import mozilla.components.support.test.argumentCaptor
import mozilla.components.support.test.eq
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.whenever
import mozilla.components.test.ReflectionUtils
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
import org.mockito.Mockito.times
import org.mockito.Mockito.verify
import org.mozilla.geckoview.AllowOrDeny
import org.mozilla.geckoview.Autocomplete
import org.mozilla.geckoview.GeckoResult
import org.mozilla.geckoview.GeckoRuntime
import org.mozilla.geckoview.GeckoSession
import org.mozilla.geckoview.GeckoSession.PromptDelegate.DateTimePrompt.Type.DATE
import org.mozilla.geckoview.GeckoSession.PromptDelegate.DateTimePrompt.Type.DATETIME_LOCAL
import org.mozilla.geckoview.GeckoSession.PromptDelegate.DateTimePrompt.Type.MONTH
import org.mozilla.geckoview.GeckoSession.PromptDelegate.DateTimePrompt.Type.TIME
import org.mozilla.geckoview.GeckoSession.PromptDelegate.DateTimePrompt.Type.WEEK
import org.mozilla.geckoview.GeckoSession.PromptDelegate.FilePrompt.Capture.ANY
import org.mozilla.geckoview.GeckoSession.PromptDelegate.FilePrompt.Capture.NONE
import org.mozilla.geckoview.GeckoSession.PromptDelegate.FilePrompt.Capture.USER
import org.robolectric.Shadows.shadowOf
import java.io.FileInputStream
import java.security.InvalidParameterException
import java.util.Calendar
import java.util.Calendar.YEAR
import java.util.Date

typealias GeckoChoice = GeckoSession.PromptDelegate.ChoicePrompt.Choice
typealias GECKO_AUTH_LEVEL = GeckoSession.PromptDelegate.AuthPrompt.AuthOptions.Level
typealias GECKO_PROMPT_CHOICE_TYPE = GeckoSession.PromptDelegate.ChoicePrompt.Type
typealias GECKO_AUTH_FLAGS = GeckoSession.PromptDelegate.AuthPrompt.AuthOptions.Flags
typealias GECKO_PROMPT_FILE_TYPE = GeckoSession.PromptDelegate.FilePrompt.Type
typealias AC_AUTH_METHOD = PromptRequest.Authentication.Method
typealias AC_AUTH_LEVEL = PromptRequest.Authentication.Level

@RunWith(AndroidJUnit4::class)
class GeckoPromptDelegateTest {

    private lateinit var runtime: GeckoRuntime

    @Before
    fun setup() {
        runtime = mock()
        whenever(runtime.settings).thenReturn(mock())
    }

    @Test
    fun `onChoicePrompt called with CHOICE_TYPE_SINGLE must provide a SingleChoice PromptRequest`() {
        val mockSession = GeckoEngineSession(runtime)
        var promptRequestSingleChoice: PromptRequest = MultipleChoice(arrayOf(), {}, {})
        var confirmWasCalled = false
        val gecko = GeckoPromptDelegate(mockSession)
        val geckoChoice = object : GeckoChoice() {}
        val geckoPrompt = geckoChoicePrompt(
            "title",
            "message",
            GECKO_PROMPT_CHOICE_TYPE.SINGLE,
            arrayOf(geckoChoice),
        )

        mockSession.register(
            object : EngineSession.Observer {
                override fun onPromptRequest(promptRequest: PromptRequest) {
                    promptRequestSingleChoice = promptRequest
                }
            },
        )

        val geckoResult = gecko.onChoicePrompt(mock(), geckoPrompt)

        geckoResult!!.accept {
            confirmWasCalled = true
        }

        assertTrue(promptRequestSingleChoice is SingleChoice)
        val request = promptRequestSingleChoice as SingleChoice

        request.onConfirm(request.choices.first())
        shadowOf(getMainLooper()).idle()
        assertTrue(confirmWasCalled)
        whenever(geckoPrompt.isComplete).thenReturn(true)

        confirmWasCalled = false
        request.onConfirm(request.choices.first())
        shadowOf(getMainLooper()).idle()
        assertFalse(confirmWasCalled)
    }

    @Test
    fun `onChoicePrompt called with CHOICE_TYPE_MULTIPLE must provide a MultipleChoice PromptRequest`() {
        val mockSession = GeckoEngineSession(runtime)
        var promptRequestSingleChoice: PromptRequest = SingleChoice(arrayOf(), {}, {})
        var confirmWasCalled = false
        val gecko = GeckoPromptDelegate(mockSession)
        val mockGeckoChoice = object : GeckoChoice() {}
        val geckoPrompt = geckoChoicePrompt(
            "title",
            "message",
            GECKO_PROMPT_CHOICE_TYPE.MULTIPLE,
            arrayOf(mockGeckoChoice),
        )

        mockSession.register(
            object : EngineSession.Observer {
                override fun onPromptRequest(promptRequest: PromptRequest) {
                    promptRequestSingleChoice = promptRequest
                }
            },
        )

        val geckoResult = gecko.onChoicePrompt(mock(), geckoPrompt)

        geckoResult!!.accept {
            confirmWasCalled = true
        }

        assertTrue(promptRequestSingleChoice is MultipleChoice)

        (promptRequestSingleChoice as MultipleChoice).onConfirm(arrayOf())
        shadowOf(getMainLooper()).idle()
        assertTrue(confirmWasCalled)
        whenever(geckoPrompt.isComplete).thenReturn(true)

        confirmWasCalled = false
        (promptRequestSingleChoice as MultipleChoice).onConfirm(arrayOf())
        shadowOf(getMainLooper()).idle()
        assertFalse(confirmWasCalled)
    }

    @Test
    fun `onChoicePrompt called with CHOICE_TYPE_MENU must provide a MenuChoice PromptRequest`() {
        val mockSession = GeckoEngineSession(runtime)
        var promptRequestSingleChoice: PromptRequest = PromptRequest.MenuChoice(arrayOf(), {}, {})
        var confirmWasCalled = false
        val gecko = GeckoPromptDelegate(mockSession)
        val geckoChoice = object : GeckoChoice() {}
        val geckoPrompt = geckoChoicePrompt(
            "title",
            "message",
            GECKO_PROMPT_CHOICE_TYPE.MENU,
            arrayOf(geckoChoice),
        )

        mockSession.register(
            object : EngineSession.Observer {
                override fun onPromptRequest(promptRequest: PromptRequest) {
                    promptRequestSingleChoice = promptRequest
                }
            },
        )

        val geckoResult = gecko.onChoicePrompt(mock(), geckoPrompt)
        geckoResult!!.accept {
            confirmWasCalled = true
        }

        assertTrue(promptRequestSingleChoice is PromptRequest.MenuChoice)
        val request = promptRequestSingleChoice as PromptRequest.MenuChoice

        request.onConfirm(request.choices.first())
        shadowOf(getMainLooper()).idle()
        assertTrue(confirmWasCalled)
        whenever(geckoPrompt.isComplete).thenReturn(true)

        confirmWasCalled = false
        request.onConfirm(request.choices.first())
        shadowOf(getMainLooper()).idle()
        assertFalse(confirmWasCalled)
    }

    @Test(expected = InvalidParameterException::class)
    fun `calling onChoicePrompt with not valid Gecko ChoiceType will throw an exception`() {
        val promptDelegate = GeckoPromptDelegate(mock())
        val geckoPrompt = geckoChoicePrompt(
            "title",
            "message",
            -1,
            arrayOf(),
        )
        promptDelegate.onChoicePrompt(mock(), geckoPrompt)
    }

    @Test
    fun `onAlertPrompt must provide an alert PromptRequest`() {
        val mockSession = GeckoEngineSession(runtime)
        var alertRequest: PromptRequest? = null
        var dismissWasCalled = false

        val promptDelegate = GeckoPromptDelegate(mockSession)

        mockSession.register(
            object : EngineSession.Observer {
                override fun onPromptRequest(promptRequest: PromptRequest) {
                    alertRequest = promptRequest
                }
            },
        )

        val geckoResult = promptDelegate.onAlertPrompt(mock(), geckoAlertPrompt())
        geckoResult.accept {
            dismissWasCalled = true
        }
        assertTrue(alertRequest is PromptRequest.Alert)

        (alertRequest as PromptRequest.Alert).onDismiss()
        shadowOf(getMainLooper()).idle()
        assertTrue(dismissWasCalled)

        assertEquals((alertRequest as PromptRequest.Alert).title, "title")
        assertEquals((alertRequest as PromptRequest.Alert).message, "message")
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
        val mockSession = GeckoEngineSession(runtime)
        var dateRequest: PromptRequest? = null
        var geckoPrompt = geckoDateTimePrompt("title", DATE, "", "", "")

        val promptDelegate = GeckoPromptDelegate(mockSession)
        mockSession.register(
            object : EngineSession.Observer {
                override fun onPromptRequest(promptRequest: PromptRequest) {
                    dateRequest = promptRequest
                }
            },
        )

        promptDelegate.onDateTimePrompt(mock(), geckoPrompt)

        assertTrue(dateRequest is PromptRequest.TimeSelection)
        val date = Date()
        (dateRequest as PromptRequest.TimeSelection).onConfirm(date)
        verify(geckoPrompt, times(1)).confirm(eq(date.toString("yyyy-MM-dd")))
        assertEquals((dateRequest as PromptRequest.TimeSelection).title, "title")

        geckoPrompt = geckoDateTimePrompt("title", DATE, "", "", "")
        promptDelegate.onDateTimePrompt(mock(), geckoPrompt)

        (dateRequest as PromptRequest.TimeSelection).onClear()
        verify(geckoPrompt, times(1)).confirm(eq(""))
    }

    @Test
    fun `onDateTimePrompt DATETIME_TYPE_DATE with date parameters must format dates correctly`() {
        val mockSession = GeckoEngineSession(runtime)
        var timeSelectionRequest: PromptRequest.TimeSelection? = null
        val confirmCaptor = argumentCaptor<String>()

        val geckoPrompt =
            geckoDateTimePrompt(
                title = "title",
                type = DATE,
                defaultValue = "2019-11-29",
                minValue = "2019-11-28",
                maxValue = "2019-11-30",
            )
        val promptDelegate = GeckoPromptDelegate(mockSession)
        mockSession.register(
            object : EngineSession.Observer {
                override fun onPromptRequest(promptRequest: PromptRequest) {
                    timeSelectionRequest = promptRequest as PromptRequest.TimeSelection
                }
            },
        )

        promptDelegate.onDateTimePrompt(mock(), geckoPrompt)

        assertNotNull(timeSelectionRequest)
        with(timeSelectionRequest!!) {
            assertEquals(initialDate, "2019-11-29".toDate("yyyy-MM-dd"))
            assertEquals(minimumDate, "2019-11-28".toDate("yyyy-MM-dd"))
            assertEquals(maximumDate, "2019-11-30".toDate("yyyy-MM-dd"))
        }
        val selectedDate = "2019-11-28".toDate("yyyy-MM-dd")
        (timeSelectionRequest as PromptRequest.TimeSelection).onConfirm(selectedDate)
        verify(geckoPrompt).confirm(confirmCaptor.capture())
        assertEquals(confirmCaptor.value.toDate("yyyy-MM-dd"), selectedDate)
        assertEquals((timeSelectionRequest as PromptRequest.TimeSelection).title, "title")
    }

    @Test
    fun `onDateTimePrompt called with DATETIME_TYPE_MONTH must provide a date PromptRequest`() {
        val mockSession = GeckoEngineSession(runtime)
        var dateRequest: PromptRequest? = null
        var confirmCalled = false

        val promptDelegate = GeckoPromptDelegate(mockSession)
        mockSession.register(
            object : EngineSession.Observer {
                override fun onPromptRequest(promptRequest: PromptRequest) {
                    dateRequest = promptRequest
                }
            },
        )
        val geckoPrompt = geckoDateTimePrompt(type = MONTH)

        val geckoResult = promptDelegate.onDateTimePrompt(mock(), geckoPrompt)
        geckoResult!!.accept {
            confirmCalled = true
        }

        shadowOf(getMainLooper()).idle()

        assertTrue(dateRequest is PromptRequest.TimeSelection)
        (dateRequest as PromptRequest.TimeSelection).onConfirm(Date())
        shadowOf(getMainLooper()).idle()

        assertTrue(confirmCalled)
        assertEquals((dateRequest as PromptRequest.TimeSelection).title, "title")
    }

    @Test
    fun `onDateTimePrompt DATETIME_TYPE_MONTH with date parameters must format dates correctly`() {
        val mockSession = GeckoEngineSession(runtime)
        var timeSelectionRequest: PromptRequest.TimeSelection? = null
        val confirmCaptor = argumentCaptor<String>()

        val promptDelegate = GeckoPromptDelegate(mockSession)
        mockSession.register(
            object : EngineSession.Observer {
                override fun onPromptRequest(promptRequest: PromptRequest) {
                    timeSelectionRequest = promptRequest as PromptRequest.TimeSelection
                }
            },
        )
        val geckoPrompt = geckoDateTimePrompt(
            title = "title",
            type = MONTH,
            defaultValue = "2019-11",
            minValue = "2019-11",
            maxValue = "2019-11",
        )
        promptDelegate.onDateTimePrompt(mock(), geckoPrompt)

        assertNotNull(timeSelectionRequest)
        with(timeSelectionRequest!!) {
            assertEquals(initialDate, "2019-11".toDate("yyyy-MM"))
            assertEquals(minimumDate, "2019-11".toDate("yyyy-MM"))
            assertEquals(maximumDate, "2019-11".toDate("yyyy-MM"))
        }
        val selectedDate = "2019-11".toDate("yyyy-MM")
        (timeSelectionRequest as PromptRequest.TimeSelection).onConfirm(selectedDate)
        verify(geckoPrompt).confirm(confirmCaptor.capture())
        assertEquals(confirmCaptor.value.toDate("yyyy-MM"), selectedDate)
        assertEquals((timeSelectionRequest as PromptRequest.TimeSelection).title, "title")
    }

    @Test
    fun `onDateTimePrompt called with DATETIME_TYPE_WEEK must provide a date PromptRequest`() {
        val mockSession = GeckoEngineSession(runtime)
        var dateRequest: PromptRequest? = null
        var confirmCalled = false
        val promptDelegate = GeckoPromptDelegate(mockSession)
        mockSession.register(
            object : EngineSession.Observer {
                override fun onPromptRequest(promptRequest: PromptRequest) {
                    dateRequest = promptRequest
                }
            },
        )
        val geckoPrompt = geckoDateTimePrompt(type = WEEK)

        val geckoResult = promptDelegate.onDateTimePrompt(mock(), geckoPrompt)
        geckoResult!!.accept {
            confirmCalled = true
        }

        shadowOf(getMainLooper()).idle()

        assertTrue(dateRequest is PromptRequest.TimeSelection)
        (dateRequest as PromptRequest.TimeSelection).onConfirm(Date())
        shadowOf(getMainLooper()).idle()
        assertTrue(confirmCalled)
        assertEquals((dateRequest as PromptRequest.TimeSelection).title, "title")
    }

    @Test
    fun `onDateTimePrompt DATETIME_TYPE_WEEK with date parameters must format dates correctly`() {
        val mockSession = GeckoEngineSession(runtime)
        var timeSelectionRequest: PromptRequest.TimeSelection? = null
        val confirmCaptor = argumentCaptor<String>()
        val promptDelegate = GeckoPromptDelegate(mockSession)
        mockSession.register(
            object : EngineSession.Observer {
                override fun onPromptRequest(promptRequest: PromptRequest) {
                    timeSelectionRequest = promptRequest as PromptRequest.TimeSelection
                }
            },
        )

        val geckoPrompt = geckoDateTimePrompt(
            title = "title",
            type = WEEK,
            defaultValue = "2018-W18",
            minValue = "2018-W18",
            maxValue = "2018-W26",
        )
        promptDelegate.onDateTimePrompt(mock(), geckoPrompt)

        assertNotNull(timeSelectionRequest)
        with(timeSelectionRequest!!) {
            assertEquals(initialDate, "2018-W18".toDate("yyyy-'W'ww"))
            assertEquals(minimumDate, "2018-W18".toDate("yyyy-'W'ww"))
            assertEquals(maximumDate, "2018-W26".toDate("yyyy-'W'ww"))
        }
        val selectedDate = "2018-W26".toDate("yyyy-'W'ww")
        (timeSelectionRequest as PromptRequest.TimeSelection).onConfirm(selectedDate)
        verify(geckoPrompt).confirm(confirmCaptor.capture())
        assertEquals(confirmCaptor.value.toDate("yyyy-'W'ww"), selectedDate)
        assertEquals((timeSelectionRequest as PromptRequest.TimeSelection).title, "title")
    }

    @Test
    fun `onDateTimePrompt called with DATETIME_TYPE_TIME must provide a TimeSelection PromptRequest`() {
        val mockSession = GeckoEngineSession(runtime)
        var dateRequest: PromptRequest? = null
        var confirmCalled = false

        val promptDelegate = GeckoPromptDelegate(mockSession)
        mockSession.register(
            object : EngineSession.Observer {
                override fun onPromptRequest(promptRequest: PromptRequest) {
                    dateRequest = promptRequest
                }
            },
        )
        val geckoPrompt = geckoDateTimePrompt(type = TIME)

        val geckoResult = promptDelegate.onDateTimePrompt(mock(), geckoPrompt)
        geckoResult!!.accept {
            confirmCalled = true
        }

        assertTrue(dateRequest is PromptRequest.TimeSelection)
        (dateRequest as PromptRequest.TimeSelection).onConfirm(Date())
        shadowOf(getMainLooper()).idle()
        assertTrue(confirmCalled)
        assertEquals((dateRequest as PromptRequest.TimeSelection).title, "title")
    }

    @Test
    fun `onDateTimePrompt DATETIME_TYPE_TIME with time parameters must format time correctly`() {
        val mockSession = GeckoEngineSession(runtime)
        var timeSelectionRequest: PromptRequest.TimeSelection? = null
        val confirmCaptor = argumentCaptor<String>()

        val promptDelegate = GeckoPromptDelegate(mockSession)
        mockSession.register(
            object : EngineSession.Observer {
                override fun onPromptRequest(promptRequest: PromptRequest) {
                    timeSelectionRequest = promptRequest as PromptRequest.TimeSelection
                }
            },
        )

        val geckoPrompt = geckoDateTimePrompt(
            title = "title",
            type = TIME,
            defaultValue = "17:00",
            minValue = "9:00",
            maxValue = "18:00",
        )
        promptDelegate.onDateTimePrompt(mock(), geckoPrompt)

        assertNotNull(timeSelectionRequest)
        with(timeSelectionRequest!!) {
            assertEquals(initialDate, "17:00".toDate("HH:mm"))
            assertEquals(minimumDate, "9:00".toDate("HH:mm"))
            assertEquals(maximumDate, "18:00".toDate("HH:mm"))
        }
        val selectedDate = "17:00".toDate("HH:mm")
        (timeSelectionRequest as PromptRequest.TimeSelection).onConfirm(selectedDate)
        verify(geckoPrompt).confirm(confirmCaptor.capture())
        assertEquals(confirmCaptor.value.toDate("HH:mm"), selectedDate)
        assertEquals((timeSelectionRequest as PromptRequest.TimeSelection).title, "title")
    }

    @Test
    fun `onDateTimePrompt DATETIME_TYPE_TIME with stepValue time parameter must format time correctly`() {
        val mockSession = GeckoEngineSession(runtime)
        var timeSelectionRequest: PromptRequest.TimeSelection? = null
        val confirmCaptor = argumentCaptor<String>()

        val promptDelegate = GeckoPromptDelegate(mockSession)
        mockSession.register(
            object : EngineSession.Observer {
                override fun onPromptRequest(promptRequest: PromptRequest) {
                    timeSelectionRequest = promptRequest as PromptRequest.TimeSelection
                }
            },
        )
        val minutesGeckoPrompt = geckoDateTimePrompt(
            type = TIME,
            defaultValue = "17:00",
            stepValue = "",
        )
        val secondsGeckoPrompt = geckoDateTimePrompt(
            type = TIME,
            defaultValue = "17:00:00",
            stepValue = "1",
        )
        val millisecondsGeckoPrompt = geckoDateTimePrompt(
            type = TIME,
            defaultValue = "17:00:00.000",
            stepValue = "0.1",
        )

        promptDelegate.onDateTimePrompt(mock(), minutesGeckoPrompt)

        var selectedTime = "17:00"
        assertNotNull(timeSelectionRequest)
        (timeSelectionRequest as PromptRequest.TimeSelection).onConfirm(selectedTime.toDate("HH:mm"))
        verify(minutesGeckoPrompt).confirm(confirmCaptor.capture())
        assertEquals(selectedTime, confirmCaptor.value)

        promptDelegate.onDateTimePrompt(mock(), secondsGeckoPrompt)

        selectedTime = "17:00:25"
        assertNotNull(timeSelectionRequest)
        (timeSelectionRequest as PromptRequest.TimeSelection).onConfirm(selectedTime.toDate("HH:mm:ss"))
        verify(secondsGeckoPrompt).confirm(confirmCaptor.capture())
        assertEquals(selectedTime, confirmCaptor.value)

        promptDelegate.onDateTimePrompt(mock(), millisecondsGeckoPrompt)

        selectedTime = "17:00:20.100"
        assertNotNull(timeSelectionRequest)
        (timeSelectionRequest as PromptRequest.TimeSelection).onConfirm(selectedTime.toDate("HH:mm:ss.SSS"))
        verify(millisecondsGeckoPrompt).confirm(confirmCaptor.capture())
        assertEquals(selectedTime, confirmCaptor.value)
    }

    @Test
    fun `onDateTimePrompt called with DATETIME_TYPE_DATETIME_LOCAL must provide a TimeSelection PromptRequest`() {
        val mockSession = GeckoEngineSession(runtime)
        var dateRequest: PromptRequest? = null
        var confirmCalled = false

        val promptDelegate = GeckoPromptDelegate(mockSession)
        mockSession.register(
            object : EngineSession.Observer {
                override fun onPromptRequest(promptRequest: PromptRequest) {
                    dateRequest = promptRequest
                }
            },
        )
        val geckoResult =
            promptDelegate.onDateTimePrompt(mock(), geckoDateTimePrompt(type = DATETIME_LOCAL))
        geckoResult!!.accept {
            confirmCalled = true
        }

        assertTrue(dateRequest is PromptRequest.TimeSelection)
        (dateRequest as PromptRequest.TimeSelection).onConfirm(Date())
        shadowOf(getMainLooper()).idle()

        assertTrue(confirmCalled)
        assertEquals((dateRequest as PromptRequest.TimeSelection).title, "title")
    }

    @Test
    fun `onDateTimePrompt DATETIME_TYPE_DATETIME_LOCAL with date parameters must format time correctly`() {
        val mockSession = GeckoEngineSession(runtime)
        var timeSelectionRequest: PromptRequest.TimeSelection? = null
        val confirmCaptor = argumentCaptor<String>()

        val promptDelegate = GeckoPromptDelegate(mockSession)
        mockSession.register(
            object : EngineSession.Observer {
                override fun onPromptRequest(promptRequest: PromptRequest) {
                    timeSelectionRequest = promptRequest as PromptRequest.TimeSelection
                }
            },
        )
        val geckoPrompt = geckoDateTimePrompt(
            title = "title",
            type = DATETIME_LOCAL,
            defaultValue = "2018-06-12T19:30",
            minValue = "2018-06-07T00:00",
            maxValue = "2018-06-14T00:00",
        )
        promptDelegate.onDateTimePrompt(mock(), geckoPrompt)

        assertNotNull(timeSelectionRequest)
        with(timeSelectionRequest!!) {
            assertEquals(initialDate, "2018-06-12T19:30".toDate("yyyy-MM-dd'T'HH:mm"))
            assertEquals(minimumDate, "2018-06-07T00:00".toDate("yyyy-MM-dd'T'HH:mm"))
            assertEquals(maximumDate, "2018-06-14T00:00".toDate("yyyy-MM-dd'T'HH:mm"))
        }
        val selectedDate = "2018-06-12T19:30".toDate("yyyy-MM-dd'T'HH:mm")
        (timeSelectionRequest as PromptRequest.TimeSelection).onConfirm(selectedDate)
        verify(geckoPrompt).confirm(confirmCaptor.capture())
        assertEquals(confirmCaptor.value.toDate("yyyy-MM-dd'T'HH:mm"), selectedDate)
        assertEquals((timeSelectionRequest as PromptRequest.TimeSelection).title, "title")
    }

    @Test(expected = InvalidParameterException::class)
    fun `Calling onDateTimePrompt with invalid DatetimeType will throw an exception`() {
        val promptDelegate = GeckoPromptDelegate(mock())
        promptDelegate.onDateTimePrompt(
            mock(),
            geckoDateTimePrompt(
                type = 13223,
                defaultValue = "17:00",
                minValue = "9:00",
                maxValue = "18:00",
            ),
        )
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

    @Test
    fun `Calling onFilePrompt must provide a FilePicker PromptRequest`() {
        val context = spy(testContext)
        val contentResolver = spy(context.contentResolver)
        val mockSession = GeckoEngineSession(runtime)
        var onSingleFileSelectedWasCalled = false
        var onMultipleFilesSelectedWasCalled = false
        var onDismissWasCalled = false
        val mockUri: Uri = mock()

        doReturn(contentResolver).`when`(context).contentResolver
        doReturn(mock<FileInputStream>()).`when`(contentResolver).openInputStream(any())

        var filePickerRequest: PromptRequest.File = mock()

        val promptDelegate = spy(GeckoPromptDelegate(mockSession))

        // Prevent the file from being copied
        doReturn(0L).`when`(promptDelegate).copyFile(any(), any())

        mockSession.register(
            object : EngineSession.Observer {
                override fun onPromptRequest(promptRequest: PromptRequest) {
                    filePickerRequest = promptRequest as PromptRequest.File
                }
            },
        )
        var geckoPrompt = geckoFilePrompt(type = GECKO_PROMPT_FILE_TYPE.SINGLE, capture = NONE)

        var geckoResult = promptDelegate.onFilePrompt(mock(), geckoPrompt)
        geckoResult!!.accept {
            onSingleFileSelectedWasCalled = true
        }

        filePickerRequest.onSingleFileSelected(context, mockUri)
        shadowOf(getMainLooper()).idle()

        assertTrue(onSingleFileSelectedWasCalled)
        whenever(geckoPrompt.isComplete).thenReturn(true)

        onSingleFileSelectedWasCalled = false
        filePickerRequest.onSingleFileSelected(context, mockUri)
        shadowOf(getMainLooper()).idle()

        assertFalse(onSingleFileSelectedWasCalled)

        geckoPrompt = geckoFilePrompt(type = GECKO_PROMPT_FILE_TYPE.MULTIPLE, capture = ANY)
        geckoResult = promptDelegate.onFilePrompt(mock(), geckoPrompt)
        geckoResult!!.accept {
            onMultipleFilesSelectedWasCalled = true
        }

        filePickerRequest.onMultipleFilesSelected(context, arrayOf(mockUri))
        shadowOf(getMainLooper()).idle()

        assertTrue(onMultipleFilesSelectedWasCalled)

        geckoPrompt = geckoFilePrompt(type = GECKO_PROMPT_FILE_TYPE.SINGLE, capture = NONE)
        geckoResult = promptDelegate.onFilePrompt(mock(), geckoPrompt)
        geckoResult!!.accept {
            onDismissWasCalled = true
        }

        filePickerRequest.onDismiss()
        shadowOf(getMainLooper()).idle()

        assertTrue(onDismissWasCalled)

        assertTrue(filePickerRequest.mimeTypes.isEmpty())
        assertFalse(filePickerRequest.isMultipleFilesSelection)
        assertEquals(PromptRequest.File.FacingMode.NONE, filePickerRequest.captureMode)

        promptDelegate.onFilePrompt(
            mock(),
            geckoFilePrompt(type = GECKO_PROMPT_FILE_TYPE.MULTIPLE, capture = USER),
        )

        assertTrue(filePickerRequest.isMultipleFilesSelection)
        assertEquals(
            PromptRequest.File.FacingMode.FRONT_CAMERA,
            filePickerRequest.captureMode,
        )
    }

    @Test
    fun `Calling onLoginSave must provide an SaveLoginPrompt PromptRequest`() {
        val mockSession = GeckoEngineSession(runtime)
        var onLoginSaved = false
        var onDismissWasCalled = false

        var loginSaveRequest: PromptRequest.SaveLoginPrompt = mock()

        val promptDelegate = spy(GeckoPromptDelegate(mockSession))

        mockSession.register(
            object : EngineSession.Observer {
                override fun onPromptRequest(promptRequest: PromptRequest) {
                    loginSaveRequest = promptRequest as PromptRequest.SaveLoginPrompt
                }
            },
        )

        val entry = createLoginEntry()
        val saveOption = Autocomplete.LoginSaveOption(entry.toLoginEntry())

        var geckoResult =
            promptDelegate.onLoginSave(mock(), geckoLoginSavePrompt(arrayOf(saveOption)))

        geckoResult!!.accept {
            onDismissWasCalled = true
        }

        loginSaveRequest.onDismiss()
        shadowOf(getMainLooper()).idle()

        assertTrue(onDismissWasCalled)

        val geckoPrompt = geckoLoginSavePrompt(arrayOf(saveOption))
        geckoResult = promptDelegate.onLoginSave(mock(), geckoPrompt)

        geckoResult!!.accept {
            onLoginSaved = true
        }

        loginSaveRequest.onConfirm(entry)
        shadowOf(getMainLooper()).idle()

        assertTrue(onLoginSaved)
        whenever(geckoPrompt.isComplete).thenReturn(true)

        onLoginSaved = false

        loginSaveRequest.onConfirm(entry)
        shadowOf(getMainLooper()).idle()

        assertFalse(onLoginSaved)
    }

    @Test
    fun `Calling onLoginSave must set a PromptInstanceDismissDelegate`() {
        val mockSession = GeckoEngineSession(runtime)
        var loginSaveRequest: PromptRequest.SaveLoginPrompt = mock()
        val promptDelegate = spy(GeckoPromptDelegate(mockSession))
        mockSession.register(
            object : EngineSession.Observer {
                override fun onPromptRequest(promptRequest: PromptRequest) {
                    loginSaveRequest = promptRequest as PromptRequest.SaveLoginPrompt
                }
            },
        )
        val login = createLogin()
        val saveOption = Autocomplete.LoginSaveOption(login.toLoginEntry())
        val saveLoginPrompt = geckoLoginSavePrompt(arrayOf(saveOption))

        promptDelegate.onLoginSave(mock(), saveLoginPrompt)

        assertNotNull(loginSaveRequest)
        assertNotNull(saveLoginPrompt.delegate)
    }

    @Test
    fun `Calling onLoginSelect must provide an SelectLoginPrompt PromptRequest`() {
        val mockSession = GeckoEngineSession(runtime)
        var onLoginSelected = false
        var onDismissWasCalled = false

        var loginSelectRequest: PromptRequest.SelectLoginPrompt = mock()

        val promptDelegate = spy(GeckoPromptDelegate(mockSession))

        mockSession.register(
            object : EngineSession.Observer {
                override fun onPromptRequest(promptRequest: PromptRequest) {
                    loginSelectRequest = promptRequest as PromptRequest.SelectLoginPrompt
                }
            },
        )

        val login = createLogin()
        val loginSelectOption = Autocomplete.LoginSelectOption(login.toLoginEntry())

        val secondLogin = createLogin(username = "username2")
        val secondLoginSelectOption = Autocomplete.LoginSelectOption(secondLogin.toLoginEntry())

        var geckoResult =
            promptDelegate.onLoginSelect(
                mock(),
                geckoLoginSelectPrompt(arrayOf(loginSelectOption, secondLoginSelectOption)),
            )

        geckoResult!!.accept {
            onDismissWasCalled = true
        }

        loginSelectRequest.onDismiss()
        shadowOf(getMainLooper()).idle()
        assertTrue(onDismissWasCalled)

        val geckoPrompt = geckoLoginSelectPrompt(arrayOf(loginSelectOption, secondLoginSelectOption))
        geckoResult = promptDelegate.onLoginSelect(
            mock(),
            geckoPrompt,
        )

        geckoResult!!.accept {
            onLoginSelected = true
        }

        loginSelectRequest.onConfirm(login)
        shadowOf(getMainLooper()).idle()

        assertTrue(onLoginSelected)
        whenever(geckoPrompt.isComplete).thenReturn(true)

        onLoginSelected = false
        loginSelectRequest.onConfirm(login)
        shadowOf(getMainLooper()).idle()

        assertFalse(onLoginSelected)
    }

    fun createLogin(
        guid: String = "id",
        password: String = "password",
        username: String = "username",
        origin: String = "https://www.origin.com",
        httpRealm: String = "httpRealm",
        formActionOrigin: String = "https://www.origin.com",
        usernameField: String = "usernameField",
        passwordField: String = "passwordField",
    ) = Login(
        guid = guid,
        origin = origin,
        password = password,
        username = username,
        httpRealm = httpRealm,
        formActionOrigin = formActionOrigin,
        usernameField = usernameField,
        passwordField = passwordField,
    )

    fun createLoginEntry(
        password: String = "password",
        username: String = "username",
        origin: String = "https://www.origin.com",
        httpRealm: String = "httpRealm",
        formActionOrigin: String = "https://www.origin.com",
        usernameField: String = "usernameField",
        passwordField: String = "passwordField",
    ) = LoginEntry(
        origin = origin,
        password = password,
        username = username,
        httpRealm = httpRealm,
        formActionOrigin = formActionOrigin,
        usernameField = usernameField,
        passwordField = passwordField,
    )

    @Test
    fun `Calling onCreditCardSave must provide an SaveCreditCard PromptRequest`() {
        val mockSession = GeckoEngineSession(runtime)
        var onCreditCardSaved = false
        var onDismissWasCalled = false

        var saveCreditCardPrompt: PromptRequest.SaveCreditCard = mock()

        val promptDelegate = spy(GeckoPromptDelegate(mockSession))

        mockSession.register(
            object : EngineSession.Observer {
                override fun onPromptRequest(promptRequest: PromptRequest) {
                    saveCreditCardPrompt = promptRequest as PromptRequest.SaveCreditCard
                }
            },
        )

        val creditCard = CreditCardEntry(
            guid = "1",
            name = "Banana Apple",
            number = "4111111111111110",
            expiryMonth = "5",
            expiryYear = "2030",
            cardType = "amex",
        )
        val creditCardSaveOption =
            Autocomplete.CreditCardSaveOption(creditCard.toAutocompleteCreditCard())

        var geckoResult = promptDelegate.onCreditCardSave(
            mock(),
            geckoCreditCardSavePrompt(arrayOf(creditCardSaveOption)),
        )

        geckoResult.accept {
            onDismissWasCalled = true
        }

        saveCreditCardPrompt.onDismiss()
        shadowOf(getMainLooper()).idle()
        assertTrue(onDismissWasCalled)

        val geckoPrompt = geckoCreditCardSavePrompt(arrayOf(creditCardSaveOption))
        geckoResult = promptDelegate.onCreditCardSave(mock(), geckoPrompt)

        geckoResult.accept {
            onCreditCardSaved = true
        }

        saveCreditCardPrompt.onConfirm(creditCard)
        shadowOf(getMainLooper()).idle()

        assertTrue(onCreditCardSaved)

        whenever(geckoPrompt.isComplete).thenReturn(true)
        onCreditCardSaved = false
        saveCreditCardPrompt.onConfirm(creditCard)

        assertFalse(onCreditCardSaved)
    }

    @Test
    fun `Calling onCreditSave must set a PromptInstanceDismissDelegate`() {
        val mockSession = GeckoEngineSession(runtime)
        var saveCreditCardPrompt: PromptRequest.SaveCreditCard = mock()
        val promptDelegate = spy(GeckoPromptDelegate(mockSession))

        mockSession.register(
            object : EngineSession.Observer {
                override fun onPromptRequest(promptRequest: PromptRequest) {
                    saveCreditCardPrompt = promptRequest as PromptRequest.SaveCreditCard
                }
            },
        )

        val creditCard = CreditCardEntry(
            guid = "1",
            name = "Banana Apple",
            number = "4111111111111110",
            expiryMonth = "5",
            expiryYear = "2030",
            cardType = "amex",
        )
        val creditCardSaveOption =
            Autocomplete.CreditCardSaveOption(creditCard.toAutocompleteCreditCard())
        val geckoPrompt = geckoCreditCardSavePrompt(arrayOf(creditCardSaveOption))

        promptDelegate.onCreditCardSave(mock(), geckoPrompt)

        assertNotNull(saveCreditCardPrompt)
        assertNotNull(geckoPrompt.delegate)
    }

    @Test
    fun `Calling onCreditCardSelect must provide as CreditCardSelectOption PromptRequest`() {
        val mockSession = GeckoEngineSession(runtime)
        var onConfirmWasCalled = false
        var onDismissWasCalled = false

        var selectCreditCardPrompt: PromptRequest.SelectCreditCard = mock()

        val promptDelegate = spy(GeckoPromptDelegate(mockSession))

        mockSession.register(
            object : EngineSession.Observer {
                override fun onPromptRequest(promptRequest: PromptRequest) {
                    selectCreditCardPrompt = promptRequest as PromptRequest.SelectCreditCard
                }
            },
        )

        val creditCard1 = CreditCardEntry(
            guid = "1",
            name = "Banana Apple",
            number = "4111111111111110",
            expiryMonth = "5",
            expiryYear = "2030",
            cardType = "amex",
        )
        val creditCardSelectOption1 =
            Autocomplete.CreditCardSelectOption(creditCard1.toAutocompleteCreditCard())

        val creditCard2 = CreditCardEntry(
            guid = "2",
            name = "Orange Pineapple",
            number = "4111111111115555",
            expiryMonth = "1",
            expiryYear = "2040",
            cardType = "amex",
        )
        val creditCardSelectOption2 =
            Autocomplete.CreditCardSelectOption(creditCard2.toAutocompleteCreditCard())

        var geckoResult = promptDelegate.onCreditCardSelect(
            mock(),
            geckoSelectCreditCardPrompt(arrayOf(creditCardSelectOption1, creditCardSelectOption2)),
        )

        geckoResult!!.accept {
            onDismissWasCalled = true
        }

        selectCreditCardPrompt.onDismiss()
        shadowOf(getMainLooper()).idle()
        assertTrue(onDismissWasCalled)

        val geckoPrompt =
            geckoSelectCreditCardPrompt(arrayOf(creditCardSelectOption1, creditCardSelectOption2))
        geckoResult = promptDelegate.onCreditCardSelect(mock(), geckoPrompt)

        geckoResult!!.accept {
            onConfirmWasCalled = true
        }

        selectCreditCardPrompt.onConfirm(creditCard1)
        shadowOf(getMainLooper()).idle()

        assertTrue(onConfirmWasCalled)

        whenever(geckoPrompt.isComplete).thenReturn(true)
        onConfirmWasCalled = false
        selectCreditCardPrompt.onConfirm(creditCard1)

        assertFalse(onConfirmWasCalled)
    }

    @Test
    fun `Calling onAuthPrompt must provide an Authentication PromptRequest`() {
        val mockSession = GeckoEngineSession(runtime)
        var authRequest: PromptRequest.Authentication = mock()

        val promptDelegate = GeckoPromptDelegate(mockSession)
        mockSession.register(
            object : EngineSession.Observer {
                override fun onPromptRequest(promptRequest: PromptRequest) {
                    authRequest = promptRequest as PromptRequest.Authentication
                }
            },
        )

        var geckoPrompt = geckoAuthPrompt(authOptions = mock())
        promptDelegate.onAuthPrompt(mock(), geckoPrompt)

        authRequest.onConfirm("", "")
        verify(geckoPrompt, times(1)).confirm(eq(""), eq(""))

        geckoPrompt = geckoAuthPrompt(authOptions = mock())
        promptDelegate.onAuthPrompt(mock(), geckoPrompt)
        authRequest.onDismiss()
        verify(geckoPrompt, times(1)).dismiss()

        val authOptions = geckoAuthOptions()
        ReflectionUtils.setField(authOptions, "level", GECKO_AUTH_LEVEL.SECURE)

        var flags = 0
        flags = flags.or(GECKO_AUTH_FLAGS.ONLY_PASSWORD)
        flags = flags.or(GECKO_AUTH_FLAGS.PREVIOUS_FAILED)
        flags = flags.or(GECKO_AUTH_FLAGS.CROSS_ORIGIN_SUB_RESOURCE)
        flags = flags.or(GECKO_AUTH_FLAGS.HOST)
        ReflectionUtils.setField(authOptions, "flags", flags)

        geckoPrompt = geckoAuthPrompt(authOptions = authOptions)
        promptDelegate.onAuthPrompt(mock(), geckoPrompt)

        authRequest.onConfirm("", "")

        with(authRequest) {
            assertTrue(onlyShowPassword)
            assertTrue(previousFailed)
            assertTrue(isCrossOrigin)

            assertEquals(method, AC_AUTH_METHOD.HOST)
            assertEquals(level, AC_AUTH_LEVEL.SECURED)

            verify(geckoPrompt, never()).confirm(eq(""), eq(""))
            verify(geckoPrompt, times(1)).confirm(eq(""))
        }

        ReflectionUtils.setField(authOptions, "level", GECKO_AUTH_LEVEL.PW_ENCRYPTED)

        promptDelegate.onAuthPrompt(mock(), geckoAuthPrompt(authOptions = authOptions))

        assertEquals(authRequest.level, AC_AUTH_LEVEL.PASSWORD_ENCRYPTED)

        ReflectionUtils.setField(authOptions, "level", -2423)

        promptDelegate.onAuthPrompt(mock(), geckoAuthPrompt(authOptions = authOptions))

        assertEquals(authRequest.level, AC_AUTH_LEVEL.NONE)
    }

    @Test
    fun `Calling onColorPrompt must provide a Color PromptRequest`() {
        val mockSession = GeckoEngineSession(runtime)
        var colorRequest: PromptRequest.Color = mock()
        var onConfirmWasCalled = false
        var onDismissWasCalled = false

        val promptDelegate = GeckoPromptDelegate(mockSession)
        mockSession.register(
            object : EngineSession.Observer {
                override fun onPromptRequest(promptRequest: PromptRequest) {
                    colorRequest = promptRequest as PromptRequest.Color
                }
            },
        )

        val geckoPrompt = geckoColorPrompt(defaultValue = "#e66465")
        var geckoResult = promptDelegate.onColorPrompt(mock(), geckoPrompt)
        geckoResult!!.accept {
            onConfirmWasCalled = true
        }

        with(colorRequest) {
            assertEquals(defaultColor, "#e66465")
            onConfirm("#f6b73c")
            shadowOf(getMainLooper()).idle()
            assertTrue(onConfirmWasCalled)
            whenever(geckoPrompt.isComplete).thenReturn(true)

            onConfirmWasCalled = false
            onConfirm("#f6b73c")
            shadowOf(getMainLooper()).idle()
            assertFalse(onConfirmWasCalled)
        }

        geckoResult = promptDelegate.onColorPrompt(mock(), geckoColorPrompt())
        geckoResult!!.accept {
            onDismissWasCalled = true
        }

        colorRequest.onDismiss()
        shadowOf(getMainLooper()).idle()
        assertTrue(onDismissWasCalled)

        with(colorRequest) {
            assertEquals(defaultColor, "defaultValue")
        }
    }

    @Test
    fun `onTextPrompt must provide an TextPrompt PromptRequest`() {
        val mockSession = GeckoEngineSession(runtime)
        var request: PromptRequest.TextPrompt = mock()
        var dismissWasCalled = false
        var confirmWasCalled = false

        val promptDelegate = GeckoPromptDelegate(mockSession)

        mockSession.register(
            object : EngineSession.Observer {
                override fun onPromptRequest(promptRequest: PromptRequest) {
                    request = promptRequest as PromptRequest.TextPrompt
                }
            },
        )

        var geckoResult = promptDelegate.onTextPrompt(mock(), geckoTextPrompt())
        geckoResult!!.accept {
            dismissWasCalled = true
        }

        with(request) {
            assertEquals(title, "title")
            assertEquals(inputLabel, "message")
            assertEquals(inputValue, "defaultValue")

            onDismiss()
            shadowOf(getMainLooper()).idle()
            assertTrue(dismissWasCalled)
        }

        val geckoPrompt = geckoTextPrompt()
        geckoResult = promptDelegate.onTextPrompt(mock(), geckoPrompt)
        geckoResult!!.accept {
            confirmWasCalled = true
        }

        request.onConfirm(true, "newInput")
        shadowOf(getMainLooper()).idle()
        assertTrue(confirmWasCalled)
        whenever(geckoPrompt.isComplete).thenReturn(true)

        confirmWasCalled = false
        request.onConfirm(true, "newInput")
        shadowOf(getMainLooper()).idle()
        assertFalse(confirmWasCalled)
    }

    @Test
    fun `onPopupRequest must provide a Popup PromptRequest`() {
        val mockSession = GeckoEngineSession(runtime)
        var request: PromptRequest.Popup? = null

        val promptDelegate = GeckoPromptDelegate(mockSession)

        mockSession.register(
            object : EngineSession.Observer {
                override fun onPromptRequest(promptRequest: PromptRequest) {
                    request = promptRequest as PromptRequest.Popup
                }
            },
        )

        var geckoPrompt = geckoPopupPrompt(targetUri = "www.popuptest.com/")
        promptDelegate.onPopupPrompt(mock(), geckoPrompt)

        with(request!!) {
            assertEquals(targetUri, "www.popuptest.com/")

            onAllow()
            verify(geckoPrompt, times(1)).confirm(eq(AllowOrDeny.ALLOW))
            whenever(geckoPrompt.isComplete).thenReturn(true)

            onAllow()
            verify(geckoPrompt, times(1)).confirm(eq(AllowOrDeny.ALLOW))
        }

        geckoPrompt = geckoPopupPrompt()
        promptDelegate.onPopupPrompt(mock(), geckoPrompt)

        request!!.onDeny()
        verify(geckoPrompt, times(1)).confirm(eq(AllowOrDeny.DENY))
        whenever(geckoPrompt.isComplete).thenReturn(true)

        request!!.onDeny()
        verify(geckoPrompt, times(1)).confirm(eq(AllowOrDeny.DENY))
    }

    @Test
    fun `onBeforeUnloadPrompt must provide a BeforeUnload PromptRequest`() {
        val mockSession = GeckoEngineSession(runtime)
        var request: PromptRequest.BeforeUnload? = null
        val promptDelegate = GeckoPromptDelegate(mockSession)

        mockSession.register(
            object : EngineSession.Observer {
                override fun onPromptRequest(promptRequest: PromptRequest) {
                    request = promptRequest as PromptRequest.BeforeUnload
                }
            },
        )

        var geckoPrompt = geckoBeforeUnloadPrompt()
        promptDelegate.onBeforeUnloadPrompt(mock(), geckoPrompt)
        assertEquals(request!!.title, "")

        request!!.onLeave()
        verify(geckoPrompt, times(1)).confirm(eq(AllowOrDeny.ALLOW))
        whenever(geckoPrompt.isComplete).thenReturn(true)

        request!!.onLeave()
        verify(geckoPrompt, times(1)).confirm(eq(AllowOrDeny.ALLOW))

        geckoPrompt = geckoBeforeUnloadPrompt()
        promptDelegate.onBeforeUnloadPrompt(mock(), geckoPrompt)

        request!!.onStay()
        verify(geckoPrompt, times(1)).confirm(eq(AllowOrDeny.DENY))
        whenever(geckoPrompt.isComplete).thenReturn(true)

        request!!.onStay()
        verify(geckoPrompt, times(1)).confirm(eq(AllowOrDeny.DENY))
    }

    @Test
    fun `onBeforeUnloadPrompt will inform listeners when if navigation is cancelled`() {
        val mockSession = GeckoEngineSession(runtime)
        var onBeforeUnloadPromptCancelledCalled = false
        var request: PromptRequest.BeforeUnload = mock()

        mockSession.register(
            object : EngineSession.Observer {
                override fun onPromptRequest(promptRequest: PromptRequest) {
                    request = promptRequest as PromptRequest.BeforeUnload
                }

                override fun onBeforeUnloadPromptDenied() {
                    onBeforeUnloadPromptCancelledCalled = true
                }
            },
        )
        val prompt = geckoBeforeUnloadPrompt()
        doReturn(false).`when`(prompt).isComplete

        GeckoPromptDelegate(mockSession).onBeforeUnloadPrompt(mock(), prompt)
        request.onStay()

        assertTrue(onBeforeUnloadPromptCancelledCalled)
    }

    @Test
    fun `onSharePrompt must provide a Share PromptRequest`() {
        val mockSession = GeckoEngineSession(runtime)
        var request: PromptRequest.Share? = null
        var onSuccessWasCalled = false
        var onFailureWasCalled = false
        var onDismissWasCalled = false

        val promptDelegate = GeckoPromptDelegate(mockSession)

        mockSession.register(
            object : EngineSession.Observer {
                override fun onPromptRequest(promptRequest: PromptRequest) {
                    request = promptRequest as PromptRequest.Share
                }
            },
        )

        var geckoPrompt = geckoSharePrompt()
        var geckoResult = promptDelegate.onSharePrompt(mock(), geckoPrompt)
        geckoResult.accept {
            onSuccessWasCalled = true
        }

        with(request!!) {
            assertEquals(data.title, "title")
            assertEquals(data.text, "text")
            assertEquals(data.url, "https://example.com")

            onSuccess()
            shadowOf(getMainLooper()).idle()
            assertTrue(onSuccessWasCalled)
            whenever(geckoPrompt.isComplete).thenReturn(true)

            onSuccessWasCalled = false
            onSuccess()
            shadowOf(getMainLooper()).idle()
            assertFalse(onSuccessWasCalled)
        }

        geckoPrompt = geckoSharePrompt()
        geckoResult = promptDelegate.onSharePrompt(mock(), geckoPrompt)
        geckoResult.accept {
            onFailureWasCalled = true
        }

        request!!.onFailure()
        shadowOf(getMainLooper()).idle()
        assertTrue(onFailureWasCalled)
        whenever(geckoPrompt.isComplete).thenReturn(true)

        onFailureWasCalled = false
        request!!.onFailure()
        shadowOf(getMainLooper()).idle()

        assertFalse(onFailureWasCalled)

        geckoPrompt = geckoSharePrompt()
        geckoResult = promptDelegate.onSharePrompt(mock(), geckoPrompt)
        geckoResult.accept {
            onDismissWasCalled = true
        }

        request!!.onDismiss()
        shadowOf(getMainLooper()).idle()
        assertTrue(onDismissWasCalled)
    }

    @Test
    fun `onButtonPrompt must provide a Confirm PromptRequest`() {
        val mockSession = GeckoEngineSession(runtime)
        var request: PromptRequest.Confirm = mock()
        var onPositiveButtonWasCalled = false
        var onNegativeButtonWasCalled = false
        var onNeutralButtonWasCalled = false
        var dismissWasCalled = false

        val promptDelegate = GeckoPromptDelegate(mockSession)

        mockSession.register(
            object : EngineSession.Observer {
                override fun onPromptRequest(promptRequest: PromptRequest) {
                    request = promptRequest as PromptRequest.Confirm
                }
            },
        )

        var geckoPrompt = geckoButtonPrompt()
        var geckoResult = promptDelegate.onButtonPrompt(mock(), geckoPrompt)
        geckoResult!!.accept {
            onPositiveButtonWasCalled = true
        }

        with(request) {
            assertNotNull(request)
            assertEquals(title, "title")
            assertEquals(message, "message")

            onConfirmPositiveButton(false)
            shadowOf(getMainLooper()).idle()
            assertTrue(onPositiveButtonWasCalled)

            whenever(geckoPrompt.isComplete).thenReturn(true)
            onPositiveButtonWasCalled = false
            onConfirmPositiveButton(false)
            shadowOf(getMainLooper()).idle()

            assertFalse(onPositiveButtonWasCalled)
        }

        geckoPrompt = geckoButtonPrompt()
        geckoResult = promptDelegate.onButtonPrompt(mock(), geckoPrompt)
        geckoResult!!.accept {
            onNeutralButtonWasCalled = true
        }

        request.onConfirmNeutralButton(false)
        shadowOf(getMainLooper()).idle()
        assertTrue(onNeutralButtonWasCalled)

        geckoPrompt = geckoButtonPrompt()
        geckoResult = promptDelegate.onButtonPrompt(mock(), geckoPrompt)
        geckoResult!!.accept {
            onNegativeButtonWasCalled = true
        }

        request.onConfirmNegativeButton(false)
        shadowOf(getMainLooper()).idle()
        assertTrue(onNegativeButtonWasCalled)
        whenever(geckoPrompt.isComplete).thenReturn(true)

        onNegativeButtonWasCalled = false
        request.onConfirmNegativeButton(false)
        shadowOf(getMainLooper()).idle()

        assertFalse(onNegativeButtonWasCalled)

        geckoResult = promptDelegate.onButtonPrompt(mock(), geckoButtonPrompt())
        geckoResult!!.accept {
            dismissWasCalled = true
        }

        request.onDismiss()
        shadowOf(getMainLooper()).idle()
        assertTrue(dismissWasCalled)
    }

    @Test
    fun `onRepostConfirmPrompt must provide a Repost PromptRequest`() {
        val mockSession = GeckoEngineSession(runtime)
        var request: PromptRequest.Repost = mock()
        var onPositiveButtonWasCalled = false
        var onNegativeButtonWasCalled = false

        mockSession.register(
            object : EngineSession.Observer {
                override fun onPromptRequest(promptRequest: PromptRequest) {
                    request = promptRequest as PromptRequest.Repost
                }
            },
        )

        val promptDelegate = GeckoPromptDelegate(mockSession)

        var geckoPrompt = geckoRepostPrompt()
        var geckoResult = promptDelegate.onRepostConfirmPrompt(mock(), geckoPrompt)
        geckoResult!!.accept {
            onPositiveButtonWasCalled = true
        }
        request.onConfirm()
        shadowOf(getMainLooper()).idle()
        assertTrue(onPositiveButtonWasCalled)
        whenever(geckoPrompt.isComplete).thenReturn(true)

        onPositiveButtonWasCalled = false
        request.onConfirm()
        shadowOf(getMainLooper()).idle()

        assertFalse(onPositiveButtonWasCalled)

        geckoPrompt = geckoRepostPrompt()
        geckoResult = promptDelegate.onRepostConfirmPrompt(mock(), geckoPrompt)
        geckoResult!!.accept {
            onNegativeButtonWasCalled = true
        }
        request.onDismiss()
        shadowOf(getMainLooper()).idle()
        assertTrue(onNegativeButtonWasCalled)
        whenever(geckoPrompt.isComplete).thenReturn(true)

        onNegativeButtonWasCalled = false
        request.onDismiss()
        shadowOf(getMainLooper()).idle()

        assertFalse(onNegativeButtonWasCalled)
    }

    @Test
    fun `onRepostConfirmPrompt will not be able to complete multiple times`() {
        val mockSession = GeckoEngineSession(runtime)
        var request: PromptRequest.Repost = mock()

        mockSession.register(
            object : EngineSession.Observer {
                override fun onPromptRequest(promptRequest: PromptRequest) {
                    request = promptRequest as PromptRequest.Repost
                }
            },
        )

        val promptDelegate = GeckoPromptDelegate(mockSession)

        var prompt = geckoRepostPrompt()
        promptDelegate.onRepostConfirmPrompt(mock(), prompt)
        doReturn(false).`when`(prompt).isComplete
        request.onConfirm()
        verify(prompt).confirm(any())

        prompt = mock()
        promptDelegate.onRepostConfirmPrompt(mock(), prompt)
        doReturn(true).`when`(prompt).isComplete
        request.onConfirm()
        verify(prompt, never()).confirm(any())

        prompt = mock()
        promptDelegate.onRepostConfirmPrompt(mock(), prompt)
        doReturn(false).`when`(prompt).isComplete
        request.onDismiss()
        verify(prompt).confirm(any())

        prompt = mock()
        promptDelegate.onRepostConfirmPrompt(mock(), prompt)
        doReturn(true).`when`(prompt).isComplete
        request.onDismiss()
        verify(prompt, never()).confirm(any())
    }

    @Test
    fun `onRepostConfirmPrompt will inform listeners when it is being dismissed`() {
        val mockSession = GeckoEngineSession(runtime)
        var onRepostPromptCancelledCalled = false
        var request: PromptRequest.Repost = mock()

        mockSession.register(
            object : EngineSession.Observer {
                override fun onPromptRequest(promptRequest: PromptRequest) {
                    request = promptRequest as PromptRequest.Repost
                }

                override fun onRepostPromptCancelled() {
                    onRepostPromptCancelledCalled = true
                }
            },
        )
        val prompt = geckoRepostPrompt()
        doReturn(false).`when`(prompt).isComplete

        GeckoPromptDelegate(mockSession).onRepostConfirmPrompt(mock(), prompt)
        request.onDismiss()

        assertTrue(onRepostPromptCancelledCalled)
    }

    @Test
    fun `dismissSafely only dismiss if the prompt is NOT already dismissed`() {
        val prompt = geckoAlertPrompt()
        val geckoResult = mock<GeckoResult<GeckoSession.PromptDelegate.PromptResponse>>()

        doReturn(false).`when`(prompt).isComplete

        prompt.dismissSafely(geckoResult)

        verify(geckoResult).complete(any())
    }

    @Test
    fun `dismissSafely do nothing if the prompt is already dismissed`() {
        val prompt = geckoAlertPrompt()
        val geckoResult = mock<GeckoResult<GeckoSession.PromptDelegate.PromptResponse>>()

        doReturn(true).`when`(prompt).isComplete

        prompt.dismissSafely(geckoResult)

        verify(geckoResult, never()).complete(any())
    }

    @Test
    fun `WHEN onAddressSelect is called THEN SelectAddress prompt request must be provided with the correct callbacks`() {
        val mockSession = GeckoEngineSession(runtime)

        var isOnConfirmCalled = false
        var isOnDismissCalled = false

        var selectAddressPrompt: PromptRequest.SelectAddress = mock()

        val promptDelegate = spy(GeckoPromptDelegate(mockSession))

        // Capture the SelectAddress prompt request
        mockSession.register(
            object : EngineSession.Observer {
                override fun onPromptRequest(promptRequest: PromptRequest) {
                    selectAddressPrompt = promptRequest as PromptRequest.SelectAddress
                }
            },
        )

        val address = Address(
            guid = "1",
            givenName = "Firefox",
            additionalName = "-",
            familyName = "-",
            organization = "-",
            streetAddress = "street",
            addressLevel3 = "address3",
            addressLevel2 = "address2",
            addressLevel1 = "address1",
            postalCode = "1",
            country = "Country",
            tel = "1",
            email = "@",
        )
        val addressSelectOption =
            Autocomplete.AddressSelectOption(address.toAutocompleteAddress())

        var geckoPrompt =
            geckoSelectAddressPrompt(arrayOf(addressSelectOption))

        var geckoResult = promptDelegate.onAddressSelect(
            mock(),
            geckoPrompt,
        )

        // Verify that the onDismiss callback was called
        geckoResult.accept {
            isOnDismissCalled = true
        }

        selectAddressPrompt.onDismiss()
        shadowOf(getMainLooper()).idle()
        assertTrue(isOnDismissCalled)

        // Verify that the onConfirm callback was called
        geckoPrompt =
            geckoSelectAddressPrompt(arrayOf(addressSelectOption))

        geckoResult = promptDelegate.onAddressSelect(
            mock(),
            geckoPrompt,
        )

        geckoResult.accept {
            isOnConfirmCalled = true
        }

        selectAddressPrompt.onConfirm(selectAddressPrompt.addresses.first())
        shadowOf(getMainLooper()).idle()
        assertTrue(isOnConfirmCalled)

        // Verify that when the prompt request is already completed and onConfirm callback is called,
        // then onConfirm callback is not executed
        isOnConfirmCalled = false
        geckoPrompt =
            geckoSelectAddressPrompt(arrayOf(addressSelectOption), true)

        geckoResult = promptDelegate.onAddressSelect(
            mock(),
            geckoPrompt,
        )

        geckoResult.accept {
            isOnConfirmCalled = true
        }

        selectAddressPrompt.onConfirm(selectAddressPrompt.addresses.first())
        shadowOf(getMainLooper()).idle()
        assertFalse(isOnConfirmCalled)
    }

    private fun geckoChoicePrompt(
        title: String,
        message: String,
        type: Int,
        choices: Array<out GeckoChoice>,
    ): GeckoSession.PromptDelegate.ChoicePrompt {
        val prompt: GeckoSession.PromptDelegate.ChoicePrompt = mock()
        ReflectionUtils.setField(prompt, "title", title)
        ReflectionUtils.setField(prompt, "type", type)
        ReflectionUtils.setField(prompt, "message", message)
        ReflectionUtils.setField(prompt, "choices", choices)
        return prompt
    }

    private fun geckoAlertPrompt(
        title: String = "title",
        message: String = "message",
    ): GeckoSession.PromptDelegate.AlertPrompt {
        val prompt: GeckoSession.PromptDelegate.AlertPrompt = mock()
        ReflectionUtils.setField(prompt, "title", title)
        ReflectionUtils.setField(prompt, "message", message)
        return prompt
    }

    private fun geckoDateTimePrompt(
        title: String = "title",
        type: Int,
        defaultValue: String = "",
        minValue: String = "",
        maxValue: String = "",
        stepValue: String = "",
    ): GeckoSession.PromptDelegate.DateTimePrompt {
        val prompt: GeckoSession.PromptDelegate.DateTimePrompt = mock()
        ReflectionUtils.setField(prompt, "title", title)
        ReflectionUtils.setField(prompt, "type", type)
        ReflectionUtils.setField(prompt, "defaultValue", defaultValue)
        ReflectionUtils.setField(prompt, "minValue", minValue)
        ReflectionUtils.setField(prompt, "maxValue", maxValue)
        ReflectionUtils.setField(prompt, "stepValue", stepValue)
        return prompt
    }

    private fun geckoFilePrompt(
        title: String = "title",
        type: Int,
        capture: Int = 0,
        mimeTypes: Array<out String> = emptyArray(),
    ): GeckoSession.PromptDelegate.FilePrompt {
        val prompt: GeckoSession.PromptDelegate.FilePrompt = mock()
        ReflectionUtils.setField(prompt, "title", title)
        ReflectionUtils.setField(prompt, "type", type)
        ReflectionUtils.setField(prompt, "capture", capture)
        ReflectionUtils.setField(prompt, "mimeTypes", mimeTypes)
        return prompt
    }

    private fun geckoAuthPrompt(
        title: String = "title",
        message: String = "message",
        authOptions: GeckoSession.PromptDelegate.AuthPrompt.AuthOptions,
    ): GeckoSession.PromptDelegate.AuthPrompt {
        val prompt: GeckoSession.PromptDelegate.AuthPrompt = mock()
        ReflectionUtils.setField(prompt, "title", title)
        ReflectionUtils.setField(prompt, "message", message)
        ReflectionUtils.setField(prompt, "authOptions", authOptions)
        return prompt
    }

    private fun geckoColorPrompt(
        title: String = "title",
        defaultValue: String = "defaultValue",
    ): GeckoSession.PromptDelegate.ColorPrompt {
        val prompt: GeckoSession.PromptDelegate.ColorPrompt = mock()
        ReflectionUtils.setField(prompt, "title", title)
        ReflectionUtils.setField(prompt, "defaultValue", defaultValue)
        return prompt
    }

    private fun geckoTextPrompt(
        title: String = "title",
        message: String = "message",
        defaultValue: String = "defaultValue",
    ): GeckoSession.PromptDelegate.TextPrompt {
        val prompt: GeckoSession.PromptDelegate.TextPrompt = mock()
        ReflectionUtils.setField(prompt, "title", title)
        ReflectionUtils.setField(prompt, "message", message)
        ReflectionUtils.setField(prompt, "defaultValue", defaultValue)
        return prompt
    }

    private fun geckoPopupPrompt(
        targetUri: String = "targetUri",
    ): GeckoSession.PromptDelegate.PopupPrompt {
        val prompt: GeckoSession.PromptDelegate.PopupPrompt = mock()
        ReflectionUtils.setField(prompt, "targetUri", targetUri)
        return prompt
    }

    private fun geckoBeforeUnloadPrompt(): GeckoSession.PromptDelegate.BeforeUnloadPrompt {
        return mock()
    }

    private fun geckoSharePrompt(
        title: String? = "title",
        text: String? = "text",
        url: String? = "https://example.com",
    ): GeckoSession.PromptDelegate.SharePrompt {
        val prompt: GeckoSession.PromptDelegate.SharePrompt = mock()
        ReflectionUtils.setField(prompt, "title", title)
        ReflectionUtils.setField(prompt, "text", text)
        ReflectionUtils.setField(prompt, "uri", url)
        return prompt
    }

    private fun geckoButtonPrompt(
        title: String = "title",
        message: String = "message",
    ): GeckoSession.PromptDelegate.ButtonPrompt {
        val prompt: GeckoSession.PromptDelegate.ButtonPrompt = mock()
        ReflectionUtils.setField(prompt, "title", title)
        ReflectionUtils.setField(prompt, "message", message)
        return prompt
    }

    private fun geckoLoginSelectPrompt(
        loginArray: Array<Autocomplete.LoginSelectOption>,
    ): GeckoSession.PromptDelegate.AutocompleteRequest<Autocomplete.LoginSelectOption> {
        val prompt: GeckoSession.PromptDelegate.AutocompleteRequest<Autocomplete.LoginSelectOption> = mock()
        ReflectionUtils.setField(prompt, "options", loginArray)
        return prompt
    }

    @Suppress("UNCHECKED_CAST")
    private fun geckoLoginSavePrompt(
        login: Array<Autocomplete.LoginSaveOption>,
    ): GeckoSession.PromptDelegate.AutocompleteRequest<Autocomplete.LoginSaveOption> {
        val prompt = Mockito.mock(
            GeckoSession.PromptDelegate.AutocompleteRequest::class.java,
            Mockito.RETURNS_DEEP_STUBS, // for testing prompt.delegate
        ) as GeckoSession.PromptDelegate.AutocompleteRequest<Autocomplete.LoginSaveOption>

        ReflectionUtils.setField(prompt, "options", login)
        return prompt
    }

    private fun geckoAuthOptions(): GeckoSession.PromptDelegate.AuthPrompt.AuthOptions {
        return mock()
    }

    private fun geckoRepostPrompt(): GeckoSession.PromptDelegate.RepostConfirmPrompt {
        return mock()
    }

    private fun geckoSelectCreditCardPrompt(
        creditCards: Array<Autocomplete.CreditCardSelectOption>,
    ): GeckoSession.PromptDelegate.AutocompleteRequest<Autocomplete.CreditCardSelectOption> {
        val prompt: GeckoSession.PromptDelegate.AutocompleteRequest<Autocomplete.CreditCardSelectOption> =
            mock()
        ReflectionUtils.setField(prompt, "options", creditCards)
        return prompt
    }

    private fun geckoSelectAddressPrompt(
        addresses: Array<Autocomplete.AddressSelectOption>,
        isComplete: Boolean = false,
    ): GeckoSession.PromptDelegate.AutocompleteRequest<Autocomplete.AddressSelectOption> {
        val prompt: GeckoSession.PromptDelegate.AutocompleteRequest<Autocomplete.AddressSelectOption> =
            mock()
        whenever(prompt.isComplete).thenReturn(isComplete)
        ReflectionUtils.setField(prompt, "options", addresses)
        return prompt
    }

    @Suppress("UNCHECKED_CAST")
    private fun geckoCreditCardSavePrompt(
        creditCard: Array<Autocomplete.CreditCardSaveOption>,
    ): GeckoSession.PromptDelegate.AutocompleteRequest<Autocomplete.CreditCardSaveOption> {
        val prompt = Mockito.mock(
            GeckoSession.PromptDelegate.AutocompleteRequest::class.java,
            Mockito.RETURNS_DEEP_STUBS, // for testing prompt.delegate
        ) as GeckoSession.PromptDelegate.AutocompleteRequest<Autocomplete.CreditCardSaveOption>

        ReflectionUtils.setField(prompt, "options", creditCard)
        return prompt
    }
}
