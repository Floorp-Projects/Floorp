/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.gecko.prompt

import android.net.Uri
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.engine.gecko.GeckoEngineSession
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.prompt.Choice
import mozilla.components.concept.engine.prompt.PromptRequest
import mozilla.components.concept.engine.prompt.PromptRequest.MultipleChoice
import mozilla.components.concept.engine.prompt.PromptRequest.SingleChoice
import mozilla.components.support.ktx.kotlin.toDate
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.test.ReflectionUtils
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.gecko.util.GeckoBundle
import org.mozilla.geckoview.GeckoSession
import java.security.InvalidParameterException
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

    @Test
    fun `onChoicePrompt called with CHOICE_TYPE_SINGLE must provide a SingleChoice PromptRequest`() {
        val mockSession = GeckoEngineSession(mock())
        var promptRequestSingleChoice: PromptRequest = MultipleChoice(arrayOf()) {}
        var confirmWasCalled = false
        val gecko = GeckoPromptDelegate(mockSession)
        val geckoChoice = object : GeckoChoice() {}
        val geckoPrompt = GeckoChoicePrompt(
            "title",
            "message",
            GECKO_PROMPT_CHOICE_TYPE.SINGLE,
            arrayOf(geckoChoice)
        )

        mockSession.register(object : EngineSession.Observer {
            override fun onPromptRequest(promptRequest: PromptRequest) {
                promptRequestSingleChoice = promptRequest
            }
        })

        val geckoResult = gecko.onChoicePrompt(mock(), geckoPrompt)

        geckoResult!!.accept {
            confirmWasCalled = true
        }

        assertTrue(promptRequestSingleChoice is SingleChoice)
        val request = promptRequestSingleChoice as SingleChoice

        request.onConfirm(request.choices.first())
        assertTrue(confirmWasCalled)
    }

    @Test
    fun `onChoicePrompt called with CHOICE_TYPE_MULTIPLE must provide a MultipleChoice PromptRequest`() {
        val mockSession = GeckoEngineSession(mock())
        var promptRequestSingleChoice: PromptRequest = SingleChoice(arrayOf()) {}
        var confirmWasCalled = false
        val gecko = GeckoPromptDelegate(mockSession)
        val mockGeckoChoice = object : GeckoChoice() {}
        val geckoPrompt = GeckoChoicePrompt(
            "title",
            "message",
            GECKO_PROMPT_CHOICE_TYPE.MULTIPLE,
            arrayOf(mockGeckoChoice)
        )

        mockSession.register(object : EngineSession.Observer {
            override fun onPromptRequest(promptRequest: PromptRequest) {
                promptRequestSingleChoice = promptRequest
            }
        })

        val geckoResult = gecko.onChoicePrompt(mock(), geckoPrompt)

        geckoResult!!.accept {
            confirmWasCalled = true
        }

        assertTrue(promptRequestSingleChoice is MultipleChoice)

        (promptRequestSingleChoice as MultipleChoice).onConfirm(arrayOf())
        assertTrue(confirmWasCalled)
    }

    @Test
    fun `onChoicePrompt called with CHOICE_TYPE_MENU must provide a MenuChoice PromptRequest`() {
        val mockSession = GeckoEngineSession(mock())
        var promptRequestSingleChoice: PromptRequest = PromptRequest.MenuChoice(arrayOf()) {}
        var confirmWasCalled = false
        val gecko = GeckoPromptDelegate(mockSession)
        val geckoChoice = object : GeckoChoice() {}
        val geckoPrompt = GeckoChoicePrompt(
            "title",
            "message",
            GECKO_PROMPT_CHOICE_TYPE.MENU,
            arrayOf(geckoChoice)
        )

        mockSession.register(
                object : EngineSession.Observer {
                    override fun onPromptRequest(promptRequest: PromptRequest) {
                        promptRequestSingleChoice = promptRequest
                    }
                })

        val geckoResult = gecko.onChoicePrompt(mock(), geckoPrompt)
        geckoResult!!.accept {
            confirmWasCalled = true
        }

        assertTrue(promptRequestSingleChoice is PromptRequest.MenuChoice)
        val request = promptRequestSingleChoice as PromptRequest.MenuChoice

        request.onConfirm(request.choices.first())
        assertTrue(confirmWasCalled)
    }

    @Test(expected = InvalidParameterException::class)
    fun `calling onChoicePrompt with not valid Gecko ChoiceType will throw an exception`() {
        val promptDelegate = GeckoPromptDelegate(mock())
        val geckoPrompt = GeckoChoicePrompt(
            "title",
            "message",
            -1,
            arrayOf()
        )
        promptDelegate.onChoicePrompt(mock(), geckoPrompt)
    }

    @Test
    fun `onAlertPrompt must provide an alert PromptRequest`() {
        val mockSession = GeckoEngineSession(mock())
        var alertRequest: PromptRequest? = null
        var dismissWasCalled = false

        val promptDelegate = GeckoPromptDelegate(mockSession)

        mockSession.register(object : EngineSession.Observer {
            override fun onPromptRequest(promptRequest: PromptRequest) {
                alertRequest = promptRequest
            }
        })

        val geckoResult = promptDelegate.onAlertPrompt(mock(), GeckoAlertPrompt())
        geckoResult.accept {
            dismissWasCalled = true
        }
        assertTrue(alertRequest is PromptRequest.Alert)

        (alertRequest as PromptRequest.Alert).onDismiss()
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
        val mockSession = GeckoEngineSession(mock())
        var dateRequest: PromptRequest? = null
        var confirmCalled = false
        var onClearPicker = false
        var geckoPrompt = GeckoDateTimePrompt("title", DATE, "", "", "")

        val promptDelegate = GeckoPromptDelegate(mockSession)
        mockSession.register(object : EngineSession.Observer {
            override fun onPromptRequest(promptRequest: PromptRequest) {
                dateRequest = promptRequest
            }
        })

        var geckoResult = promptDelegate.onDateTimePrompt(mock(), geckoPrompt)
        geckoResult!!.accept {
            confirmCalled = true
        }

        assertTrue(dateRequest is PromptRequest.TimeSelection)
        (dateRequest as PromptRequest.TimeSelection).onConfirm(Date())
        assertTrue(confirmCalled)
        assertEquals((dateRequest as PromptRequest.TimeSelection).title, "title")

        geckoPrompt = GeckoDateTimePrompt("title", DATE, "", "", "")
        geckoResult = promptDelegate.onDateTimePrompt(mock(), geckoPrompt)
        geckoResult!!.accept {
            onClearPicker = true
        }

        (dateRequest as PromptRequest.TimeSelection).onClear()
        assertTrue(onClearPicker)
    }

    @Test
    fun `onDateTimePrompt DATETIME_TYPE_DATE with date parameters must format dates correctly`() {
        val mockSession = GeckoEngineSession(mock())
        var timeSelectionRequest: PromptRequest.TimeSelection? = null
        var geckoDate: String? = null

        val geckoPrompt =
            GeckoDateTimePrompt(
                title = "title",
                type = DATE,
                defaultValue = "2019-11-29",
                minValue = "2019-11-28",
                maxValue = "2019-11-30"
            )
        val promptDelegate = GeckoPromptDelegate(mockSession)
        mockSession.register(object : EngineSession.Observer {
            override fun onPromptRequest(promptRequest: PromptRequest) {
                timeSelectionRequest = promptRequest as PromptRequest.TimeSelection
            }
        })

        val geckoResult = promptDelegate.onDateTimePrompt(mock(), geckoPrompt)
        geckoResult!!.accept {
            geckoDate = geckoPrompt.getGeckoResult()["datetime"].toString()
        }

        assertNotNull(timeSelectionRequest)
        with(timeSelectionRequest!!) {
            assertEquals(initialDate, "2019-11-29".toDate("yyyy-MM-dd"))
            assertEquals(minimumDate, "2019-11-28".toDate("yyyy-MM-dd"))
            assertEquals(maximumDate, "2019-11-30".toDate("yyyy-MM-dd"))
        }
        val selectedDate = "2019-11-28".toDate("yyyy-MM-dd")
        (timeSelectionRequest as PromptRequest.TimeSelection).onConfirm(selectedDate)
        assertNotNull(geckoDate?.toDate("yyyy-MM-dd")?.equals(selectedDate))
        assertEquals((timeSelectionRequest as PromptRequest.TimeSelection).title, "title")
    }

    @Test
    fun `onDateTimePrompt called with DATETIME_TYPE_MONTH must provide a date PromptRequest`() {
        val mockSession = GeckoEngineSession(mock())
        var dateRequest: PromptRequest? = null
        var confirmCalled = false

        val promptDelegate = GeckoPromptDelegate(mockSession)
        mockSession.register(object : EngineSession.Observer {
            override fun onPromptRequest(promptRequest: PromptRequest) {
                dateRequest = promptRequest
            }
        })
        val geckoPrompt = GeckoDateTimePrompt(type = MONTH)

        val geckoResult = promptDelegate.onDateTimePrompt(mock(), geckoPrompt)
        geckoResult!!.accept {
            confirmCalled = true
        }
        assertTrue(dateRequest is PromptRequest.TimeSelection)
        (dateRequest as PromptRequest.TimeSelection).onConfirm(Date())
        assertTrue(confirmCalled)
        assertEquals((dateRequest as PromptRequest.TimeSelection).title, "title")
    }

    @Test
    fun `onDateTimePrompt DATETIME_TYPE_MONTH with date parameters must format dates correctly`() {
        val mockSession = GeckoEngineSession(mock())
        var timeSelectionRequest: PromptRequest.TimeSelection? = null
        var geckoDate: String? = null

        val promptDelegate = GeckoPromptDelegate(mockSession)
        mockSession.register(object : EngineSession.Observer {
            override fun onPromptRequest(promptRequest: PromptRequest) {
                timeSelectionRequest = promptRequest as PromptRequest.TimeSelection
            }
        })
        val geckoPrompt = GeckoDateTimePrompt(
            title = "title",
            type = MONTH,
            defaultValue = "2019-11",
            minValue = "2019-11",
            maxValue = "2019-11"
        )
        val geckoResult = promptDelegate.onDateTimePrompt(mock(), geckoPrompt)
        geckoResult!!.accept {
            geckoDate = geckoPrompt.getGeckoResult()["datetime"].toString()
        }

        assertNotNull(timeSelectionRequest)
        with(timeSelectionRequest!!) {
            assertEquals(initialDate, "2019-11".toDate("yyyy-MM"))
            assertEquals(minimumDate, "2019-11".toDate("yyyy-MM"))
            assertEquals(maximumDate, "2019-11".toDate("yyyy-MM"))
        }
        val selectedDate = "2019-11".toDate("yyyy-MM")
        (timeSelectionRequest as PromptRequest.TimeSelection).onConfirm(selectedDate)
        assertNotNull(geckoDate?.toDate("yyyy-MM")?.equals(selectedDate))
        assertEquals((timeSelectionRequest as PromptRequest.TimeSelection).title, "title")
    }

    @Test
    fun `onDateTimePrompt called with DATETIME_TYPE_WEEK must provide a date PromptRequest`() {
        val mockSession = GeckoEngineSession(mock())
        var dateRequest: PromptRequest? = null
        var confirmCalled = false
        val promptDelegate = GeckoPromptDelegate(mockSession)
        mockSession.register(object : EngineSession.Observer {
            override fun onPromptRequest(promptRequest: PromptRequest) {
                dateRequest = promptRequest
            }
        })
        val geckoPrompt = GeckoDateTimePrompt(type = WEEK)

        val geckoResult = promptDelegate.onDateTimePrompt(mock(), geckoPrompt)
        geckoResult!!.accept {
            confirmCalled = true
        }

        assertTrue(dateRequest is PromptRequest.TimeSelection)
        (dateRequest as PromptRequest.TimeSelection).onConfirm(Date())
        assertTrue(confirmCalled)
        assertEquals((dateRequest as PromptRequest.TimeSelection).title, "title")
    }

    @Test
    fun `onDateTimePrompt DATETIME_TYPE_WEEK with date parameters must format dates correctly`() {
        val mockSession = GeckoEngineSession(mock())
        var timeSelectionRequest: PromptRequest.TimeSelection? = null
        var geckoDate: String? = null
        val promptDelegate = GeckoPromptDelegate(mockSession)
        mockSession.register(object : EngineSession.Observer {
            override fun onPromptRequest(promptRequest: PromptRequest) {
                timeSelectionRequest = promptRequest as PromptRequest.TimeSelection
            }
        })

        val geckoPrompt = GeckoDateTimePrompt(
            title = "title",
            type = WEEK,
            defaultValue = "2018-W18",
            minValue = "2018-W18",
            maxValue = "2018-W26"
        )
        val geckoResult = promptDelegate.onDateTimePrompt(mock(), geckoPrompt)
        geckoResult!!.accept {
            geckoDate = geckoPrompt.getGeckoResult()["datetime"].toString()
        }

        assertNotNull(timeSelectionRequest)
        with(timeSelectionRequest!!) {
            assertEquals(initialDate, "2018-W18".toDate("yyyy-'W'ww"))
            assertEquals(minimumDate, "2018-W18".toDate("yyyy-'W'ww"))
            assertEquals(maximumDate, "2018-W26".toDate("yyyy-'W'ww"))
        }
        val selectedDate = "2018-W26".toDate("yyyy-'W'ww")
        (timeSelectionRequest as PromptRequest.TimeSelection).onConfirm(selectedDate)
        assertNotNull(geckoDate?.toDate("yyyy-'W'ww")?.equals(selectedDate))
        assertEquals((timeSelectionRequest as PromptRequest.TimeSelection).title, "title")
    }

    @Test
    fun `onDateTimePrompt called with DATETIME_TYPE_TIME must provide a TimeSelection PromptRequest`() {
        val mockSession = GeckoEngineSession(mock())
        var dateRequest: PromptRequest? = null
        var confirmCalled = false

        val promptDelegate = GeckoPromptDelegate(mockSession)
        mockSession.register(object : EngineSession.Observer {
            override fun onPromptRequest(promptRequest: PromptRequest) {
                dateRequest = promptRequest
            }
        })
        val geckoPrompt = GeckoDateTimePrompt(type = TIME)

        val geckoResult = promptDelegate.onDateTimePrompt(mock(), geckoPrompt)
        geckoResult!!.accept {
            confirmCalled = true
        }

        assertTrue(dateRequest is PromptRequest.TimeSelection)
        (dateRequest as PromptRequest.TimeSelection).onConfirm(Date())
        assertTrue(confirmCalled)
        assertEquals((dateRequest as PromptRequest.TimeSelection).title, "title")
    }

    @Test
    fun `onDateTimePrompt DATETIME_TYPE_TIME with time parameters must format time correctly`() {
        val mockSession = GeckoEngineSession(mock())
        var timeSelectionRequest: PromptRequest.TimeSelection? = null
        var geckoDate: String? = null

        val promptDelegate = GeckoPromptDelegate(mockSession)
        mockSession.register(object : EngineSession.Observer {
            override fun onPromptRequest(promptRequest: PromptRequest) {
                timeSelectionRequest = promptRequest as PromptRequest.TimeSelection
            }
        })

        val geckoPrompt = GeckoDateTimePrompt(
            title = "title",
            type = TIME,
            defaultValue = "17:00",
            minValue = "9:00",
            maxValue = "18:00"
        )
        val geckoResult = promptDelegate.onDateTimePrompt(mock(), geckoPrompt)
        geckoResult!!.accept {
            geckoDate = geckoPrompt.getGeckoResult()["datetime"].toString()
        }

        assertNotNull(timeSelectionRequest)
        with(timeSelectionRequest!!) {
            assertEquals(initialDate, "17:00".toDate("HH:mm"))
            assertEquals(minimumDate, "9:00".toDate("HH:mm"))
            assertEquals(maximumDate, "18:00".toDate("HH:mm"))
        }
        val selectedDate = "17:00".toDate("HH:mm")
        (timeSelectionRequest as PromptRequest.TimeSelection).onConfirm(selectedDate)
        assertNotNull(geckoDate?.toDate("HH:mm")?.equals(selectedDate))
        assertEquals((timeSelectionRequest as PromptRequest.TimeSelection).title, "title")
    }

    @Test
    fun `onDateTimePrompt called with DATETIME_TYPE_DATETIME_LOCAL must provide a TimeSelection PromptRequest`() {
        val mockSession = GeckoEngineSession(mock())
        var dateRequest: PromptRequest? = null
        var confirmCalled = false

        val promptDelegate = GeckoPromptDelegate(mockSession)
        mockSession.register(object : EngineSession.Observer {
            override fun onPromptRequest(promptRequest: PromptRequest) {
                dateRequest = promptRequest
            }
        })
        val geckoResult = promptDelegate.onDateTimePrompt(mock(), GeckoDateTimePrompt(type = DATETIME_LOCAL))
        geckoResult!!.accept {
            confirmCalled = true
        }

        assertTrue(dateRequest is PromptRequest.TimeSelection)
        (dateRequest as PromptRequest.TimeSelection).onConfirm(Date())
        assertTrue(confirmCalled)
        assertEquals((dateRequest as PromptRequest.TimeSelection).title, "title")
    }

    @Test
    fun `onDateTimePrompt DATETIME_TYPE_DATETIME_LOCAL with date parameters must format time correctly`() {
        val mockSession = GeckoEngineSession(mock())
        var timeSelectionRequest: PromptRequest.TimeSelection? = null
        var geckoDate: String? = null
        val promptDelegate = GeckoPromptDelegate(mockSession)
        mockSession.register(object : EngineSession.Observer {
            override fun onPromptRequest(promptRequest: PromptRequest) {
                timeSelectionRequest = promptRequest as PromptRequest.TimeSelection
            }
        })
        val geckoPrompt = GeckoDateTimePrompt(
            title = "title",
            type = DATETIME_LOCAL,
            defaultValue = "2018-06-12T19:30",
            minValue = "2018-06-07T00:00",
            maxValue = "2018-06-14T00:00"
        )
        val geckoResult = promptDelegate.onDateTimePrompt(mock(), geckoPrompt)
        geckoResult!!.accept {
            geckoDate = geckoPrompt.getGeckoResult()["datetime"].toString()
        }

        assertNotNull(timeSelectionRequest)
        with(timeSelectionRequest!!) {
            assertEquals(initialDate, "2018-06-12T19:30".toDate("yyyy-MM-dd'T'HH:mm"))
            assertEquals(minimumDate, "2018-06-07T00:00".toDate("yyyy-MM-dd'T'HH:mm"))
            assertEquals(maximumDate, "2018-06-14T00:00".toDate("yyyy-MM-dd'T'HH:mm"))
        }
        val selectedDate = "2018-06-12T19:30".toDate("yyyy-MM-dd'T'HH:mm")
        (timeSelectionRequest as PromptRequest.TimeSelection).onConfirm(selectedDate)
        assertNotNull(geckoDate?.toDate("yyyy-MM-dd'T'HH:mm")?.equals(selectedDate))
        assertEquals((timeSelectionRequest as PromptRequest.TimeSelection).title, "title")
    }

    @Test(expected = InvalidParameterException::class)
    fun `Calling onDateTimePrompt with invalid DatetimeType will throw an exception`() {
        val promptDelegate = GeckoPromptDelegate(mock())
        promptDelegate.onDateTimePrompt(
            mock(),
            GeckoDateTimePrompt(
                type = 13223,
                defaultValue = "17:00",
                minValue = "9:00",
                maxValue = "18:00"
            )
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
        val context = testContext

        val mockSession = GeckoEngineSession(mock())
        var onSingleFileSelectedWasCalled = false
        var onMultipleFilesSelectedWasCalled = false
        var onDismissWasCalled = false
        val mockUri: Uri = mock()
        val mockFileInput: FileInputStream = mock()
        val shadowContentResolver = shadowOf(context.contentResolver)

        shadowContentResolver.registerInputStream(mockUri, mockFileInput)
        var filePickerRequest: PromptRequest.File = mock()

        val promptDelegate = GeckoPromptDelegate(mockSession)
        mockSession.register(object : EngineSession.Observer {
            override fun onPromptRequest(promptRequest: PromptRequest) {
                filePickerRequest = promptRequest as PromptRequest.File
            }
        })
        var geckoPrompt = GeckoFilePrompt(type = GECKO_PROMPT_FILE_TYPE.SINGLE, capture = NONE)

        var geckoResult = promptDelegate.onFilePrompt(mock(), geckoPrompt)
        geckoResult!!.accept {
            onSingleFileSelectedWasCalled = true
        }

        filePickerRequest.onSingleFileSelected(context, mockUri)
        assertTrue(onSingleFileSelectedWasCalled)

        geckoPrompt = GeckoFilePrompt(type = GECKO_PROMPT_FILE_TYPE.MULTIPLE, capture = ANY)
        geckoResult = promptDelegate.onFilePrompt(mock(), geckoPrompt)
        geckoResult!!.accept {
            onMultipleFilesSelectedWasCalled = true
        }

        filePickerRequest.onMultipleFilesSelected(context, arrayOf(mockUri))
        assertTrue(onMultipleFilesSelectedWasCalled)

        geckoPrompt = GeckoFilePrompt(type = GECKO_PROMPT_FILE_TYPE.SINGLE, capture = NONE)
        geckoResult = promptDelegate.onFilePrompt(mock(), geckoPrompt)
        geckoResult!!.accept {
            onDismissWasCalled = true
        }

        filePickerRequest.onDismiss()
        assertTrue(onDismissWasCalled)

        assertTrue(filePickerRequest.mimeTypes.isEmpty())
        assertFalse(filePickerRequest.isMultipleFilesSelection)
        assertEquals(PromptRequest.File.FacingMode.NONE, filePickerRequest.captureMode)

        promptDelegate.onFilePrompt(
            mock(),
            GeckoFilePrompt(type = GECKO_PROMPT_FILE_TYPE.MULTIPLE, capture = USER)
        )

        assertTrue(filePickerRequest.isMultipleFilesSelection)
        assertEquals(
            PromptRequest.File.FacingMode.FRONT_CAMERA,
            filePickerRequest.captureMode
        )
    }

    @Test
    fun `Calling onAuthPrompt must provide an Authentication PromptRequest`() {
        val mockSession = GeckoEngineSession(mock())
        var authRequest: PromptRequest.Authentication = mock()
        var onConfirmWasCalled = false
        var onConfirmOnlyPasswordWasCalled = false
        var onDismissWasCalled = false

        val promptDelegate = GeckoPromptDelegate(mockSession)
        mockSession.register(object : EngineSession.Observer {
            override fun onPromptRequest(promptRequest: PromptRequest) {
                authRequest = promptRequest as PromptRequest.Authentication
            }
        })

        var geckoResult =
            promptDelegate.onAuthPrompt(mock(), GeckoAuthPrompt(authOptions = mock()))
        geckoResult!!.accept {
            onConfirmWasCalled = true
        }

        authRequest.onConfirm("", "")
        assertTrue(onConfirmWasCalled)

        geckoResult =
            promptDelegate.onAuthPrompt(mock(), GeckoAuthPrompt(authOptions = mock()))
        geckoResult!!.accept {
            onDismissWasCalled = true
        }

        authRequest.onDismiss()
        assertTrue(onDismissWasCalled)

        val authOptions = GeckoAuthOptions()
        ReflectionUtils.setField(authOptions, "level", GECKO_AUTH_LEVEL.SECURE)

        var flags = 0
        flags = flags.or(GECKO_AUTH_FLAGS.ONLY_PASSWORD)
        flags = flags.or(GECKO_AUTH_FLAGS.PREVIOUS_FAILED)
        flags = flags.or(GECKO_AUTH_FLAGS.CROSS_ORIGIN_SUB_RESOURCE)
        flags = flags.or(GECKO_AUTH_FLAGS.HOST)
        ReflectionUtils.setField(authOptions, "flags", flags)

        val geckoPrompt = GeckoAuthPrompt(authOptions = authOptions)
        geckoResult = promptDelegate.onAuthPrompt(mock(), geckoPrompt)
        geckoResult!!.accept {
            val hasPassword = geckoPrompt.getGeckoResult().containsKey("password")
            val hasUser = geckoPrompt.getGeckoResult().containsKey("username")
            onConfirmOnlyPasswordWasCalled = hasPassword && hasUser == false
        }

        authRequest.onConfirm("", "")

        with(authRequest) {
            assertTrue(onlyShowPassword)
            assertTrue(previousFailed)
            assertTrue(isCrossOrigin)

            assertEquals(method, AC_AUTH_METHOD.HOST)
            assertEquals(level, AC_AUTH_LEVEL.SECURED)
            assertTrue(onConfirmOnlyPasswordWasCalled)
        }

        ReflectionUtils.setField(authOptions, "level", GECKO_AUTH_LEVEL.PW_ENCRYPTED)

        promptDelegate.onAuthPrompt(mock(), GeckoAuthPrompt(authOptions = authOptions))

        assertEquals(authRequest.level, AC_AUTH_LEVEL.PASSWORD_ENCRYPTED)

        ReflectionUtils.setField(authOptions, "level", -2423)

        promptDelegate.onAuthPrompt(mock(), GeckoAuthPrompt(authOptions = authOptions))

        assertEquals(authRequest.level, AC_AUTH_LEVEL.NONE)
    }

    @Test
    fun `Calling onColorPrompt must provide a Color PromptRequest`() {
        val mockSession = GeckoEngineSession(mock())
        var colorRequest: PromptRequest.Color = mock()
        var onConfirmWasCalled = false
        var onDismissWasCalled = false

        val promptDelegate = GeckoPromptDelegate(mockSession)
        mockSession.register(object : EngineSession.Observer {
            override fun onPromptRequest(promptRequest: PromptRequest) {
                colorRequest = promptRequest as PromptRequest.Color
            }
        })

        var geckoResult =
            promptDelegate.onColorPrompt(mock(), GeckoColorPrompt(defaultValue = "#e66465"))
        geckoResult!!.accept {
            onConfirmWasCalled = true
        }

        with(colorRequest) {

            assertEquals(defaultColor, "#e66465")

            onConfirm("#f6b73c")
            assertTrue(onConfirmWasCalled)
        }

        geckoResult = promptDelegate.onColorPrompt(mock(), GeckoColorPrompt())
        geckoResult!!.accept {
            onDismissWasCalled = true
        }

        colorRequest.onDismiss()
        assertTrue(onDismissWasCalled)

        with(colorRequest) {
            assertEquals(defaultColor, "defaultValue")
        }
    }

    @Test
    fun `onTextPrompt must provide an TextPrompt PromptRequest`() {
        val mockSession = GeckoEngineSession(mock())
        var request: PromptRequest.TextPrompt = mock()
        var dismissWasCalled = false
        var confirmWasCalled = false

        val promptDelegate = GeckoPromptDelegate(mockSession)

        mockSession.register(object : EngineSession.Observer {
            override fun onPromptRequest(promptRequest: PromptRequest) {
                request = promptRequest as PromptRequest.TextPrompt
            }
        })

        var geckoResult = promptDelegate.onTextPrompt(mock(), GeckoTextPrompt())
        geckoResult!!.accept {
            dismissWasCalled = true
        }

        with(request) {
            assertEquals(title, "title")
            assertEquals(inputLabel, "message")
            assertEquals(inputValue, "defaultValue")

            onDismiss()
            assertTrue(dismissWasCalled)
        }

        geckoResult = promptDelegate.onTextPrompt(mock(), GeckoTextPrompt())
        geckoResult!!.accept {
            confirmWasCalled = true
        }

        request.onConfirm(true, "newInput")
        assertTrue(confirmWasCalled)
    }

    @Test
    fun `onPopupRequest must provide a Popup PromptRequest`() {
        val mockSession = GeckoEngineSession(mock())
        var request: PromptRequest.Popup? = null
        var onAllowWasCalled = false
        var onDenyWasCalled = false

        val promptDelegate = GeckoPromptDelegate(mockSession)

        mockSession.register(object : EngineSession.Observer {
            override fun onPromptRequest(promptRequest: PromptRequest) {
                request = promptRequest as PromptRequest.Popup
            }
        })

        var geckoPrompt = GeckoPopupPrompt(targetUri = "www.popuptest.com/")
        var geckoResult = promptDelegate.onPopupPrompt(mock(), geckoPrompt)
        geckoResult.accept {
            onAllowWasCalled = geckoPrompt.getGeckoResult()["response"] == true
        }

        with(request!!) {
            assertEquals(targetUri, "www.popuptest.com/")

            onAllow()
            assertTrue(onAllowWasCalled)
        }

        geckoPrompt = GeckoPopupPrompt()
        geckoResult = promptDelegate.onPopupPrompt(mock(), geckoPrompt)
        geckoResult.accept {
            onDenyWasCalled = geckoPrompt.getGeckoResult()["response"] == false
        }

        request!!.onDeny()
        assertTrue(onDenyWasCalled)
    }

    @Test
    fun `onButtonPrompt must provide a Confirm PromptRequest`() {
        val mockSession = GeckoEngineSession(mock())
        var request: PromptRequest.Confirm = mock()
        var onPositiveButtonWasCalled = false
        var onNegativeButtonWasCalled = false
        var onNeutralButtonWasCalled = false
        var dismissWasCalled = false

        val promptDelegate = GeckoPromptDelegate(mockSession)

        mockSession.register(object : EngineSession.Observer {
            override fun onPromptRequest(promptRequest: PromptRequest) {
                request = promptRequest as PromptRequest.Confirm
            }
        })

        var geckoResult = promptDelegate.onButtonPrompt(mock(), GeckoPromptPrompt())
        geckoResult!!.accept {
            onPositiveButtonWasCalled = true
        }

        with(request) {

            assertNotNull(request)
            assertEquals(title, "title")
            assertEquals(message, "message")

            onConfirmPositiveButton(false)
            assertTrue(onPositiveButtonWasCalled)
        }

        geckoResult = promptDelegate.onButtonPrompt(mock(), GeckoPromptPrompt())
        geckoResult!!.accept {
            onNeutralButtonWasCalled = true
        }

        request.onConfirmNeutralButton(false)
        assertTrue(onNeutralButtonWasCalled)

        geckoResult = promptDelegate.onButtonPrompt(mock(), GeckoPromptPrompt())
        geckoResult!!.accept {
            onNegativeButtonWasCalled = true
        }

        request.onConfirmNegativeButton(false)
        assertTrue(onNegativeButtonWasCalled)

        geckoResult = promptDelegate.onButtonPrompt(mock(), GeckoPromptPrompt())
        geckoResult!!.accept {
            dismissWasCalled = true
        }

        request.onDismiss()
        assertTrue(dismissWasCalled)
    }

    class GeckoChoicePrompt(
        title: String,
        message: String,
        type: Int,
        choices: Array<out GeckoChoice>
    ) : GeckoSession.PromptDelegate.ChoicePrompt(title, message, type, choices)

    class GeckoAlertPrompt(title: String = "title", message: String = "message") :
        GeckoSession.PromptDelegate.AlertPrompt(title, message)

    class GeckoDateTimePrompt(
        title: String = "title",
        type: Int,
        defaultValue: String = "",
        minValue: String = "",
        maxValue: String = ""
    ) : GeckoSession.PromptDelegate.DateTimePrompt(title, type, defaultValue, minValue, maxValue)

    class GeckoFilePrompt(
        title: String = "title",
        type: Int,
        capture: Int = 0,
        mimeTypes: Array<out String > = emptyArray()
    ) : GeckoSession.PromptDelegate.FilePrompt(title, type, capture, mimeTypes)

    class GeckoAuthPrompt(
        title: String = "title",
        message: String = "message",
        authOptions: AuthOptions
    ) : GeckoSession.PromptDelegate.AuthPrompt(title, message, authOptions)

    class GeckoColorPrompt(
        title: String = "title",
        defaultValue: String = "defaultValue"
    ) : GeckoSession.PromptDelegate.ColorPrompt(title, defaultValue)

    class GeckoTextPrompt(
        title: String = "title",
        message: String = "message",
        defaultValue: String = "defaultValue"
    ) : GeckoSession.PromptDelegate.TextPrompt(title, message, defaultValue)

    class GeckoPopupPrompt(
        targetUri: String = "targetUri"
    ) : GeckoSession.PromptDelegate.PopupPrompt(targetUri)

    class GeckoPromptPrompt(
        title: String = "title",
        message: String = "message"
    ) : GeckoSession.PromptDelegate.ButtonPrompt(title, message)

    class GeckoAuthOptions : GeckoSession.PromptDelegate.AuthPrompt.AuthOptions()

    private fun GeckoSession.PromptDelegate.BasePrompt.getGeckoResult(): GeckoBundle {
        val javaClass = GeckoSession.PromptDelegate.BasePrompt::class.java
        val method = javaClass.getDeclaredMethod("ensureResult")
        method.isAccessible = true
        return (method.invoke(this) as GeckoBundle)
    }
}
