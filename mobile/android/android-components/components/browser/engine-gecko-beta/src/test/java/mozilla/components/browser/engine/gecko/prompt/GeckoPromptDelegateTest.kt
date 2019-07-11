/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.gecko.prompt

import android.content.Context
import android.net.Uri
import androidx.test.core.app.ApplicationProvider
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.engine.gecko.GeckoEngineSession
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.prompt.Choice
import mozilla.components.concept.engine.prompt.PromptRequest
import mozilla.components.concept.engine.prompt.PromptRequest.Authentication.Level.NONE
import mozilla.components.concept.engine.prompt.PromptRequest.Authentication.Level.PASSWORD_ENCRYPTED
import mozilla.components.concept.engine.prompt.PromptRequest.Authentication.Level.SECURED
import mozilla.components.concept.engine.prompt.PromptRequest.Authentication.Method.HOST
import mozilla.components.concept.engine.prompt.PromptRequest.MultipleChoice
import mozilla.components.concept.engine.prompt.PromptRequest.SingleChoice
import mozilla.components.support.ktx.kotlin.toDate
import mozilla.components.support.test.mock
import org.junit.Assert
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.geckoview.AllowOrDeny
import org.mozilla.geckoview.GeckoResult
import org.mozilla.geckoview.GeckoSession
import org.mozilla.geckoview.GeckoSession.PromptDelegate.AuthOptions.AUTH_FLAG_CROSS_ORIGIN_SUB_RESOURCE
import org.mozilla.geckoview.GeckoSession.PromptDelegate.AuthOptions.AUTH_FLAG_HOST
import org.mozilla.geckoview.GeckoSession.PromptDelegate.AuthOptions.AUTH_FLAG_ONLY_PASSWORD
import org.mozilla.geckoview.GeckoSession.PromptDelegate.AuthOptions.AUTH_FLAG_PREVIOUS_FAILED
import org.mozilla.geckoview.GeckoSession.PromptDelegate.AuthOptions.AUTH_LEVEL_PW_ENCRYPTED
import org.mozilla.geckoview.GeckoSession.PromptDelegate.AuthOptions.AUTH_LEVEL_SECURE
import org.mozilla.geckoview.GeckoSession.PromptDelegate.BUTTON_TYPE_NEGATIVE
import org.mozilla.geckoview.GeckoSession.PromptDelegate.BUTTON_TYPE_NEUTRAL
import org.mozilla.geckoview.GeckoSession.PromptDelegate.BUTTON_TYPE_POSITIVE
import org.mozilla.geckoview.GeckoSession.PromptDelegate.Choice.CHOICE_TYPE_MULTIPLE
import org.mozilla.geckoview.GeckoSession.PromptDelegate.Choice.CHOICE_TYPE_SINGLE
import org.mozilla.geckoview.GeckoSession.PromptDelegate.DATETIME_TYPE_DATE
import org.mozilla.geckoview.GeckoSession.PromptDelegate.DATETIME_TYPE_DATETIME_LOCAL
import org.mozilla.geckoview.GeckoSession.PromptDelegate.DATETIME_TYPE_MONTH
import org.mozilla.geckoview.GeckoSession.PromptDelegate.DATETIME_TYPE_TIME
import org.mozilla.geckoview.GeckoSession.PromptDelegate.DATETIME_TYPE_WEEK
import org.mozilla.geckoview.GeckoSession.PromptDelegate.TextCallback
import org.robolectric.Shadows.shadowOf
import java.io.FileInputStream
import java.security.InvalidParameterException
import java.util.Calendar
import java.util.Calendar.YEAR
import java.util.Date

typealias GeckoChoice = GeckoSession.PromptDelegate.Choice

@RunWith(AndroidJUnit4::class)
class GeckoPromptDelegateTest {

    @Test
    fun `onChoicePrompt called with CHOICE_TYPE_SINGLE must provide a SingleChoice PromptRequest`() {
        val mockSession = GeckoEngineSession(mock())
        var promptRequestSingleChoice: PromptRequest = MultipleChoice(arrayOf()) {}
        var confirmWasCalled = false

        val callback = object : DefaultGeckoChoiceCallback() {
            override fun confirm(id: String?) {
                confirmWasCalled = true
            }
        }

        val gecko = GeckoPromptDelegate(mockSession)
        val geckoChoice = object : GeckoChoice() {}
        val geckoChoices = arrayOf(geckoChoice)

        mockSession.register(object : EngineSession.Observer {
            override fun onPromptRequest(promptRequest: PromptRequest) {
                promptRequestSingleChoice = promptRequest
            }
        })
        gecko.onChoicePrompt(mock(), null, null, CHOICE_TYPE_SINGLE, geckoChoices, callback)

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

        val callback = object : DefaultGeckoChoiceCallback() {
            override fun confirm(ids: Array<out String>) {
                confirmWasCalled = true
            }
        }

        val gecko = GeckoPromptDelegate(mockSession)
        val mockGeckoChoice = object : GeckoChoice() {}
        val geckoChoices = arrayOf(mockGeckoChoice)

        mockSession.register(object : EngineSession.Observer {
            override fun onPromptRequest(promptRequest: PromptRequest) {
                promptRequestSingleChoice = promptRequest
            }
        })

        gecko.onChoicePrompt(mock(), null, null, CHOICE_TYPE_MULTIPLE, geckoChoices, callback)

        assertTrue(promptRequestSingleChoice is MultipleChoice)

        (promptRequestSingleChoice as MultipleChoice).onConfirm(arrayOf())
        assertTrue(confirmWasCalled)
    }

    @Test
    fun `onChoicePrompt called with CHOICE_TYPE_MENU must provide a MenuChoice PromptRequest`() {
        val mockSession = GeckoEngineSession(mock())
        var promptRequestSingleChoice: PromptRequest = PromptRequest.MenuChoice(arrayOf()) {}
        var confirmWasCalled = false

        val callback = object : DefaultGeckoChoiceCallback() {
            override fun confirm(id: String?) {
                confirmWasCalled = true
            }
        }

        val gecko = GeckoPromptDelegate(mockSession)
        val geckoChoice = object : GeckoChoice() {}
        val geckoChoices = arrayOf(geckoChoice)

        mockSession.register(
                object : EngineSession.Observer {
                    override fun onPromptRequest(promptRequest: PromptRequest) {
                        promptRequestSingleChoice = promptRequest
                    }
                })

        gecko.onChoicePrompt(
                mock(), null, null,
                GeckoSession.PromptDelegate.Choice.CHOICE_TYPE_MENU, geckoChoices, callback
        )

        assertTrue(promptRequestSingleChoice is PromptRequest.MenuChoice)
        val request = promptRequestSingleChoice as PromptRequest.MenuChoice

        request.onConfirm(request.choices.first())
        assertTrue(confirmWasCalled)
    }

    @Test(expected = InvalidParameterException::class)
    fun `calling onChoicePrompt with not valid Gecko ChoiceType will throw an exception`() {
        val promptDelegate = GeckoPromptDelegate(mock())
        promptDelegate.onChoicePrompt(
            mock(),
            "title",
            "message",
            -1,
            arrayOf(),
            mock()
        )
    }

    @Test
    fun `onAlert must provide an alert PromptRequest`() {
        val mockSession = GeckoEngineSession(mock())
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

        promptDelegate.onAlert(mock(), "title", "message", callback)

        assertTrue(alertRequest is PromptRequest.Alert)

        (alertRequest as PromptRequest.Alert).onDismiss()
        assertTrue(dismissWasCalled)

        (alertRequest as PromptRequest.Alert).onConfirm(true)
        assertTrue(setCheckboxValueWasCalled)

        assertEquals((alertRequest as PromptRequest.Alert).title, "title")
        assertEquals((alertRequest as PromptRequest.Alert).message, "message")
    }

    @Test
    fun `hitting default values`() {
        val mockSession = GeckoEngineSession(mock())
        val gecko = GeckoPromptDelegate(mockSession)
        gecko.onDateTimePrompt(mock(), null, DATETIME_TYPE_DATE, null, null, null, mock())
        gecko.onDateTimePrompt(mock(), null, DATETIME_TYPE_WEEK, null, null, null, mock())
        gecko.onDateTimePrompt(mock(), null, DATETIME_TYPE_MONTH, null, null, null, mock())
        gecko.onDateTimePrompt(mock(), null, DATETIME_TYPE_TIME, null, "", "", mock())
        gecko.onButtonPrompt(mock(), null, null, arrayOf<String?>(null, null, null), mock())
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
        promptDelegate.onDateTimePrompt(mock(), "title", DATETIME_TYPE_DATE, "", "", "", callback)
        assertTrue(dateRequest is PromptRequest.TimeSelection)
        (dateRequest as PromptRequest.TimeSelection).onConfirm(Date())
        assertTrue(confirmCalled)
        assertEquals((dateRequest as PromptRequest.TimeSelection).title, "title")

        (dateRequest as PromptRequest.TimeSelection).onClear()
        assertTrue(onClearPicker)
    }

    @Test
    fun `onDateTimePrompt DATETIME_TYPE_DATE with date parameters must format dates correctly`() {
        val mockSession = GeckoEngineSession(mock())
        var timeSelectionRequest: PromptRequest.TimeSelection? = null
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
                timeSelectionRequest = promptRequest as PromptRequest.TimeSelection
            }
        })
        promptDelegate.onDateTimePrompt(
            mock(),
                "title",
                DATETIME_TYPE_DATE,
                "2019-11-29",
                "2019-11-28",
                "2019-11-30",
                callback
        )
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
        promptDelegate.onDateTimePrompt(mock(), "title", DATETIME_TYPE_MONTH, "", "", "", callback)
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
                timeSelectionRequest = promptRequest as PromptRequest.TimeSelection
            }
        })
        promptDelegate.onDateTimePrompt(
                mock(),
                "title",
                DATETIME_TYPE_MONTH,
                "2019-11",
                "2019-11",
                "2019-11",
                callback
        )
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
        promptDelegate.onDateTimePrompt(mock(), "title", DATETIME_TYPE_WEEK, "", "", "", callback)
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
                timeSelectionRequest = promptRequest as PromptRequest.TimeSelection
            }
        })
        promptDelegate.onDateTimePrompt(
                mock(),
                "title",
                DATETIME_TYPE_WEEK,
                "2018-W18",
                "2018-W18",
                "2018-W26",
                callback
        )
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
        promptDelegate.onDateTimePrompt(mock(), "title", DATETIME_TYPE_TIME, "", "", "", callback)
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
                timeSelectionRequest = promptRequest as PromptRequest.TimeSelection
            }
        })
        promptDelegate.onDateTimePrompt(
            mock(),
            "title",
            DATETIME_TYPE_TIME,
            "17:00",
            "9:00",
            "18:00",
            callback
        )
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
        promptDelegate.onDateTimePrompt(
            mock(), "title",
            DATETIME_TYPE_DATETIME_LOCAL, "", "", "", callback
        )
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
                timeSelectionRequest = promptRequest as PromptRequest.TimeSelection
            }
        })
        promptDelegate.onDateTimePrompt(
            mock(),
            "title",
            DATETIME_TYPE_DATETIME_LOCAL,
            "2018-06-12T19:30",
            "2018-06-07T00:00",
            "2018-06-14T00:00",
            callback
        )
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
            "title",
            13223,
            "17:00",
            "9:00",
            "18:00",
            mock()
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
        val context = ApplicationProvider.getApplicationContext<Context>()

        val mockSession = GeckoEngineSession(mock())
        var request: PromptRequest? = null
        var onSingleFileSelectedWasCalled = false
        var onMultipleFilesSelectedWasCalled = false
        var onDismissWasCalled = false
        val mockUri: Uri = mock()
        val mockFileInput: FileInputStream = mock()
        val shadowContentResolver = shadowOf(context.contentResolver)

        shadowContentResolver.registerInputStream(mockUri, mockFileInput)

        val callback = object : GeckoSession.PromptDelegate.FileCallback {
            override fun dismiss() {
                onDismissWasCalled = true
            }

            override fun confirm(context: Context?, uri: Uri?) {
                onSingleFileSelectedWasCalled = true
            }

            override fun confirm(context: Context?, uris: Array<out Uri>?) {
                onMultipleFilesSelectedWasCalled = true
            }

            override fun getCheckboxValue() = false
            override fun setCheckboxValue(value: Boolean) = Unit
            override fun hasCheckbox() = false
            override fun getCheckboxMessage() = ""
        }

        val promptDelegate = GeckoPromptDelegate(mockSession)
        mockSession.register(object : EngineSession.Observer {
            override fun onPromptRequest(promptRequest: PromptRequest) {
                request = promptRequest
            }
        })

        promptDelegate.onFilePrompt(mock(), "title", GeckoSession.PromptDelegate.FILE_TYPE_SINGLE, emptyArray(), callback)
        assertTrue(request is PromptRequest.File)

        val filePickerRequest = request as PromptRequest.File

        filePickerRequest.onSingleFileSelected(context, mockUri)
        assertTrue(onSingleFileSelectedWasCalled)

        filePickerRequest.onMultipleFilesSelected(context, arrayOf(mockUri))
        assertTrue(onMultipleFilesSelectedWasCalled)

        filePickerRequest.onDismiss()
        assertTrue(onDismissWasCalled)

        assertTrue(filePickerRequest.mimeTypes.isEmpty())
        Assert.assertFalse(filePickerRequest.isMultipleFilesSelection)

        promptDelegate.onFilePrompt(
            mock(), "title",
            GeckoSession.PromptDelegate.FILE_TYPE_MULTIPLE, emptyArray(), callback
        )

        assertTrue((request as PromptRequest.File).isMultipleFilesSelection)
    }

    @Test
    fun `Calling onAuthPrompt must provide an Authentication PromptRequest`() {
        val mockSession = GeckoEngineSession(mock())
        var request: PromptRequest? = null
        var onConfirmWasCalled = false
        var onConfirmOnlyPasswordWasCalled = false
        var onDismissWasCalled = false
        val authOptions = mock<GeckoSession.PromptDelegate.AuthOptions>()

        val geckoCallback = object : GeckoSession.PromptDelegate.AuthCallback {

            override fun confirm(username: String, password: String) {
                onConfirmWasCalled = true
            }

            override fun dismiss() {
                onDismissWasCalled = true
            }

            override fun confirm(password: String?) {
                onConfirmOnlyPasswordWasCalled = true
            }

            override fun getCheckboxValue() = false
            override fun setCheckboxValue(value: Boolean) = Unit
            override fun hasCheckbox() = false
            override fun getCheckboxMessage() = ""
        }

        val promptDelegate = GeckoPromptDelegate(mockSession)
        mockSession.register(object : EngineSession.Observer {
            override fun onPromptRequest(promptRequest: PromptRequest) {
                request = promptRequest
            }
        })

        promptDelegate.onAuthPrompt(mock(), "title", "message", authOptions, geckoCallback)
        assertTrue(request is PromptRequest.Authentication)

        var authRequest = request as PromptRequest.Authentication

        authRequest.onConfirm("", "")
        assertTrue(onConfirmWasCalled)

        authRequest.onDismiss()
        assertTrue(onDismissWasCalled)

        authOptions.level = AUTH_LEVEL_SECURE

        authOptions.flags = authOptions.flags.or(AUTH_FLAG_ONLY_PASSWORD)
        authOptions.flags = authOptions.flags.or(AUTH_FLAG_PREVIOUS_FAILED)
        authOptions.flags = authOptions.flags.or(AUTH_FLAG_CROSS_ORIGIN_SUB_RESOURCE)
        authOptions.flags = authOptions.flags.or(AUTH_FLAG_HOST)

        promptDelegate.onAuthPrompt(mock(), "title", "message", authOptions, geckoCallback)

        authRequest = request as PromptRequest.Authentication

        authRequest.onConfirm("", "")

        with(authRequest) {
            assertTrue(onlyShowPassword)
            assertTrue(previousFailed)
            assertTrue(isCrossOrigin)

            assertEquals(method, HOST)
            assertEquals(level, SECURED)
            assertTrue(onConfirmOnlyPasswordWasCalled)
        }

        authOptions.level = AUTH_LEVEL_PW_ENCRYPTED

        promptDelegate.onAuthPrompt(mock(), "title", "message", authOptions, geckoCallback)
        authRequest = request as PromptRequest.Authentication

        assertEquals(authRequest.level, PASSWORD_ENCRYPTED)

        authOptions.level = -2423

        promptDelegate.onAuthPrompt(mock(), "title", "message", authOptions, geckoCallback)
        authRequest = request as PromptRequest.Authentication

        assertEquals(authRequest.level, NONE)
    }

    @Test
    fun `Calling onColorPrompt must provide a Color PromptRequest`() {
        val mockSession = GeckoEngineSession(mock())
        var request: PromptRequest? = null
        var onConfirmWasCalled = false
        var onDismissWasCalled = false

        val geckoCallback = object : TextCallback {

            override fun confirm(text: String?) {
                onConfirmWasCalled = true
            }

            override fun dismiss() {
                onDismissWasCalled = true
            }

            override fun getCheckboxValue() = false
            override fun setCheckboxValue(value: Boolean) = Unit
            override fun hasCheckbox() = false
            override fun getCheckboxMessage() = ""
        }

        val promptDelegate = GeckoPromptDelegate(mockSession)
        mockSession.register(object : EngineSession.Observer {
            override fun onPromptRequest(promptRequest: PromptRequest) {
                request = promptRequest
            }
        })

        promptDelegate.onColorPrompt(mock(), "title", "#e66465", geckoCallback)
        assertTrue(request is PromptRequest.Color)

        var colorRequest = request as PromptRequest.Color

        with(colorRequest) {

            assertEquals(defaultColor, "#e66465")

            onConfirm("#f6b73c")
            assertTrue(onConfirmWasCalled)

            onDismiss()
            assertTrue(onDismissWasCalled)
        }

        promptDelegate.onColorPrompt(mock(), null, null, geckoCallback)
        colorRequest = request as PromptRequest.Color

        with(colorRequest) {
            assertEquals(defaultColor, "")
        }
    }

    @Test
    fun `onTextPrompt must provide an TextPrompt PromptRequest`() {
        val mockSession = GeckoEngineSession(mock())
        var request: PromptRequest? = null
        var dismissWasCalled = false
        var confirmWasCalled = false
        var setCheckboxValueWasCalled = false

        val callback = object : TextCallback {

            override fun confirm(text: String?) {
                confirmWasCalled = true
            }

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
                request = promptRequest
            }
        })

        promptDelegate.onTextPrompt(mock(), "title", "label", "value", callback)

        with(request as PromptRequest.TextPrompt) {
            assertEquals(title, "title")
            assertEquals(inputLabel, "label")
            assertEquals(inputValue, "value")

            onDismiss()
            assertTrue(dismissWasCalled)

            onConfirm(true, "newInput")
            assertTrue(setCheckboxValueWasCalled)
            assertTrue(confirmWasCalled)
        }
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

        var geckoCallback = promptDelegate.onPopupRequest(mock(), "www.popuptest.com/")

        val geckoThen: (AllowOrDeny?) -> GeckoResult<AllowOrDeny> = {
            when (it!!) {
                AllowOrDeny.ALLOW -> { onAllowWasCalled = true }
                AllowOrDeny.DENY -> { onDenyWasCalled = true }
            }
            geckoCallback
        }

        geckoCallback.then(geckoThen)

        with(request!!) {
            assertEquals(targetUri, "www.popuptest.com/")

            onAllow()
            assertTrue(onAllowWasCalled)
        }

        geckoCallback = promptDelegate.onPopupRequest(mock(), "www.popuptest.com/")
        geckoCallback.then(geckoThen)

        request!!.onDeny()
        assertTrue(onDenyWasCalled)
    }

    @Test
    fun `onButtonPrompt must provide a Confirm PromptRequest`() {
        val mockSession = GeckoEngineSession(mock())
        var request: PromptRequest.Confirm? = null
        var onPositiveButtonWasCalled = false
        var onNegativeButtonWasCalled = false
        var onNeutralButtonWasCalled = false
        var dismissWasCalled = false
        var setCheckboxValueWasCalled = false

        val promptDelegate = GeckoPromptDelegate(mockSession)

        mockSession.register(object : EngineSession.Observer {
            override fun onPromptRequest(promptRequest: PromptRequest) {
                request = promptRequest as PromptRequest.Confirm
            }
        })

        val callback = object : GeckoSession.PromptDelegate.ButtonCallback {

            override fun confirm(button: Int) {
                when (button) {
                    BUTTON_TYPE_POSITIVE -> onPositiveButtonWasCalled = true
                    BUTTON_TYPE_NEGATIVE -> onNegativeButtonWasCalled = true
                    BUTTON_TYPE_NEUTRAL -> onNeutralButtonWasCalled = true
                }
            }

            override fun setCheckboxValue(value: Boolean) {
                setCheckboxValueWasCalled = true
            }

            override fun dismiss() {
                dismissWasCalled = true
            }

            override fun getCheckboxValue(): Boolean = false
            override fun hasCheckbox(): Boolean = true
            override fun getCheckboxMessage(): String = ""
        }

        promptDelegate.onButtonPrompt(
            mock(),
            "title",
            "message",
            arrayOf("positive", "neutral", "negative"),
            callback
        )

        with(request!!) {

            assertNotNull(request)
            assertEquals(title, "title")
            assertEquals(message, "message")
            assertEquals(hasShownManyDialogs, true)
            assertEquals(positiveButtonTitle, "positive")
            assertEquals(negativeButtonTitle, "negative")
            assertEquals(neutralButtonTitle, "neutral")

            onConfirmPositiveButton(false)
            assertTrue(onPositiveButtonWasCalled)

            onConfirmNegativeButton(false)
            assertTrue(onNegativeButtonWasCalled)

            onConfirmNeutralButton(false)
            assertTrue(onNeutralButtonWasCalled)

            assertTrue(setCheckboxValueWasCalled)

            onDismiss()
            assertTrue(dismissWasCalled)
        }
    }

    open class DefaultGeckoChoiceCallback : GeckoSession.PromptDelegate.ChoiceCallback {
        override fun confirm(items: Array<out GeckoChoice>?) = Unit
        override fun dismiss() {}
        override fun getCheckboxValue() = false
        override fun setCheckboxValue(value: Boolean) = Unit
        override fun hasCheckbox() = false
        override fun getCheckboxMessage() = ""
        override fun confirm(ids: Array<out String>) = Unit
        override fun confirm(item: GeckoChoice) = Unit
        override fun confirm(id: String?) = Unit
    }
}