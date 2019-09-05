/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.gecko.prompt

import android.content.ContentResolver
import android.content.Context
import android.net.Uri
import android.provider.OpenableColumns
import androidx.annotation.VisibleForTesting
import mozilla.components.browser.engine.gecko.GeckoEngineSession
import mozilla.components.concept.engine.prompt.Choice
import mozilla.components.concept.engine.prompt.PromptRequest
import mozilla.components.concept.engine.prompt.PromptRequest.MenuChoice
import mozilla.components.concept.engine.prompt.PromptRequest.MultipleChoice
import mozilla.components.concept.engine.prompt.PromptRequest.SingleChoice
import mozilla.components.support.base.log.logger.Logger
import mozilla.components.support.ktx.kotlin.toDate
import org.mozilla.geckoview.AllowOrDeny
import org.mozilla.geckoview.GeckoResult
import org.mozilla.geckoview.GeckoSession
import org.mozilla.geckoview.GeckoSession.PromptDelegate
import org.mozilla.geckoview.GeckoSession.PromptDelegate.DateTimePrompt.Type.DATE
import org.mozilla.geckoview.GeckoSession.PromptDelegate.DateTimePrompt.Type.DATETIME_LOCAL
import org.mozilla.geckoview.GeckoSession.PromptDelegate.DateTimePrompt.Type.MONTH
import org.mozilla.geckoview.GeckoSession.PromptDelegate.DateTimePrompt.Type.TIME
import org.mozilla.geckoview.GeckoSession.PromptDelegate.DateTimePrompt.Type.WEEK
import org.mozilla.geckoview.GeckoSession.PromptDelegate.PromptResponse
import java.io.FileOutputStream
import java.io.IOException
import java.security.InvalidParameterException
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

typealias GeckoAuthOptions = PromptDelegate.AuthPrompt.AuthOptions
typealias GeckoChoice = PromptDelegate.ChoicePrompt.Choice
typealias GECKO_AUTH_FLAGS = PromptDelegate.AuthPrompt.AuthOptions.Flags
typealias GECKO_AUTH_LEVEL = PromptDelegate.AuthPrompt.AuthOptions.Level
typealias GECKO_PROMPT_FILE_TYPE = PromptDelegate.FilePrompt.Type
typealias GECKO_PROMPT_CHOICE_TYPE = PromptDelegate.ChoicePrompt.Type
typealias GECKO_PROMPT_FILE_CAPTURE = PromptDelegate.FilePrompt.Capture
typealias AC_AUTH_LEVEL = PromptRequest.Authentication.Level
typealias AC_AUTH_METHOD = PromptRequest.Authentication.Method
typealias AC_FILE_FACING_MODE = PromptRequest.File.FacingMode

/**
 * Gecko-based PromptDelegate implementation.
 */
@Suppress("TooManyFunctions")
internal class GeckoPromptDelegate(private val geckoEngineSession: GeckoEngineSession) :
    PromptDelegate {

    override fun onChoicePrompt(
        session: GeckoSession,
        geckoPrompt: PromptDelegate.ChoicePrompt
    ): GeckoResult<PromptResponse>? {
        val geckoResult = GeckoResult<PromptResponse>()
        val choices = convertToChoices(geckoPrompt.choices)
        val onConfirmSingleChoice: (Choice) -> Unit = { selectedChoice ->
            geckoResult.complete(geckoPrompt.confirm(selectedChoice.id))
        }
        val onConfirmMultipleSelection: (Array<Choice>) -> Unit = { selectedChoices ->
            val ids = selectedChoices.toIdsArray()
            geckoResult.complete(geckoPrompt.confirm(ids))
        }

        val promptRequest = when (geckoPrompt.type) {
            GECKO_PROMPT_CHOICE_TYPE.SINGLE -> SingleChoice(
                choices,
                onConfirmSingleChoice
            )
            GECKO_PROMPT_CHOICE_TYPE.MENU -> MenuChoice(
                choices,
                onConfirmSingleChoice
            )
            GECKO_PROMPT_CHOICE_TYPE.MULTIPLE -> MultipleChoice(
                choices,
                onConfirmMultipleSelection
            )
            else -> throw InvalidParameterException("${geckoPrompt.type} is not a valid Gecko @Choice.ChoiceType")
        }

        geckoEngineSession.notifyObservers {
            onPromptRequest(promptRequest)
        }

        return geckoResult
    }

    override fun onAlertPrompt(
        session: GeckoSession,
        prompt: PromptDelegate.AlertPrompt
    ): GeckoResult<PromptResponse> {
        val geckoResult = GeckoResult<PromptResponse>()
        val onConfirm: () -> Unit = { geckoResult.complete(prompt.dismiss()) }
        val title = prompt.title ?: ""
        val message = prompt.message ?: ""

        geckoEngineSession.notifyObservers {
            onPromptRequest(
                PromptRequest.Alert(
                    title,
                    message,
                    false,
                    onConfirm
                ) { _ ->
                    onConfirm()
                })
        }
        return geckoResult
    }

    override fun onFilePrompt(
        session: GeckoSession,
        prompt: PromptDelegate.FilePrompt
    ): GeckoResult<PromptResponse>? {
        val geckoResult = GeckoResult<PromptResponse>()
        val isMultipleFilesSelection = prompt.type == GECKO_PROMPT_FILE_TYPE.MULTIPLE

        val captureMode = when (prompt.capture) {
            GECKO_PROMPT_FILE_CAPTURE.ANY -> AC_FILE_FACING_MODE.ANY
            GECKO_PROMPT_FILE_CAPTURE.USER -> AC_FILE_FACING_MODE.FRONT_CAMERA
            GECKO_PROMPT_FILE_CAPTURE.ENVIRONMENT -> AC_FILE_FACING_MODE.BACK_CAMERA
            else -> AC_FILE_FACING_MODE.NONE
        }

        val onSelectMultiple: (Context, Array<Uri>) -> Unit = { context, uris ->
            val filesUris = uris.map {
                it.toFileUri(context)
            }.toTypedArray()

            geckoResult.complete(prompt.confirm(context, filesUris))
        }

        val onSelectSingle: (Context, Uri) -> Unit = { context, uri ->
            geckoResult.complete(prompt.confirm(context, uri.toFileUri(context)))
        }

        val onDismiss: () -> Unit = {
            geckoResult.complete(prompt.dismiss())
        }

        geckoEngineSession.notifyObservers {
            onPromptRequest(
                PromptRequest.File(
                    prompt.mimeTypes ?: emptyArray(),
                    isMultipleFilesSelection,
                    captureMode,
                    onSelectSingle,
                    onSelectMultiple,
                    onDismiss
                )
            )
        }
        return geckoResult
    }

    override fun onDateTimePrompt(
        session: GeckoSession,
        prompt: PromptDelegate.DateTimePrompt
    ): GeckoResult<PromptResponse>? {
        val geckoResult = GeckoResult<PromptResponse>()
        val onConfirm: (String) -> Unit = { geckoResult.complete(prompt.confirm(it)) }
        val onClear: () -> Unit = {
            onConfirm("")
        }
        val initialDateString = prompt.defaultValue ?: ""

        val format = when (prompt.type) {
            DATE -> "yyyy-MM-dd"
            MONTH -> "yyyy-MM"
            WEEK -> "yyyy-'W'ww"
            TIME -> "HH:mm"
            DATETIME_LOCAL -> "yyyy-MM-dd'T'HH:mm"
            else -> {
                throw InvalidParameterException("${prompt.type} is not a valid DatetimeType")
            }
        }

        notifyDatePromptRequest(
            prompt.title ?: "",
            initialDateString,
            prompt.minValue,
            prompt.maxValue,
            onClear,
            format,
            onConfirm
        )

        return geckoResult
    }

    override fun onAuthPrompt(
        session: GeckoSession,
        geckoPrompt: PromptDelegate.AuthPrompt
    ): GeckoResult<PromptResponse>? {
        val geckoResult = GeckoResult<PromptResponse>()
        val title = geckoPrompt.title ?: ""
        val message = geckoPrompt.message ?: ""
        val flags = geckoPrompt.authOptions.flags
        val userName = geckoPrompt.authOptions.username ?: ""
        val password = geckoPrompt.authOptions.password ?: ""
        val method =
            if (flags in GECKO_AUTH_FLAGS.HOST) AC_AUTH_METHOD.HOST else AC_AUTH_METHOD.PROXY
        val level = geckoPrompt.authOptions.toACLevel()
        val onlyShowPassword = flags in GECKO_AUTH_FLAGS.ONLY_PASSWORD
        val previousFailed = flags in GECKO_AUTH_FLAGS.PREVIOUS_FAILED
        val isCrossOrigin = flags in GECKO_AUTH_FLAGS.CROSS_ORIGIN_SUB_RESOURCE

        val onConfirm: (String, String) -> Unit =
            { user, pass ->
                if (onlyShowPassword) {
                    geckoResult.complete(geckoPrompt.confirm(pass))
                } else {
                    geckoResult.complete(geckoPrompt.confirm(user, pass))
                }
            }

        val onDismiss: () -> Unit = { geckoResult.complete(geckoPrompt.dismiss()) }

        geckoEngineSession.notifyObservers {
            onPromptRequest(
                PromptRequest.Authentication(
                    title,
                    message,
                    userName,
                    password,
                    method,
                    level,
                    onlyShowPassword,
                    previousFailed,
                    isCrossOrigin,
                    onConfirm,
                    onDismiss
                )
            )
        }
        return geckoResult
    }

    override fun onTextPrompt(
        session: GeckoSession,
        prompt: PromptDelegate.TextPrompt
    ): GeckoResult<PromptResponse>? {
        val geckoResult = GeckoResult<PromptResponse>()
        val title = prompt.title ?: ""
        val inputLabel = prompt.message ?: ""
        val inputValue = prompt.defaultValue ?: ""
        val onDismiss: () -> Unit = { geckoResult.complete(prompt.dismiss()) }

        geckoEngineSession.notifyObservers {
            onPromptRequest(
                PromptRequest.TextPrompt(
                    title,
                    inputLabel,
                    inputValue,
                    false,
                    onDismiss
                ) { _, valueInput ->
                    geckoResult.complete(prompt.confirm(valueInput))
                })
        }

        return geckoResult
    }

    override fun onColorPrompt(
        session: GeckoSession,
        prompt: PromptDelegate.ColorPrompt
    ): GeckoResult<PromptResponse>? {
        val geckoResult = GeckoResult<PromptResponse>()
        val onConfirm: (String) -> Unit = { geckoResult.complete(prompt.confirm(it)) }
        val onDismiss: () -> Unit = { geckoResult.complete(prompt.dismiss()) }

        val defaultColor = prompt.defaultValue ?: ""

        geckoEngineSession.notifyObservers {
            onPromptRequest(
                PromptRequest.Color(defaultColor, onConfirm, onDismiss)
            )
        }
        return geckoResult
    }

    override fun onPopupPrompt(
        session: GeckoSession,
        prompt: PromptDelegate.PopupPrompt
    ): GeckoResult<PromptResponse> {
        val geckoResult = GeckoResult<PromptResponse>()
        val onAllow: () -> Unit = { geckoResult.complete(prompt.confirm(AllowOrDeny.ALLOW)) }
        val onDeny: () -> Unit = { geckoResult.complete(prompt.confirm(AllowOrDeny.DENY)) }

        geckoEngineSession.notifyObservers {
            onPromptRequest(
                PromptRequest.Popup(prompt.targetUri ?: "", onAllow, onDeny)
            )
        }
        return geckoResult
    }

    override fun onButtonPrompt(
        session: GeckoSession,
        prompt: PromptDelegate.ButtonPrompt
    ): GeckoResult<PromptResponse>? {
        val geckoResult = GeckoResult<PromptResponse>()
        val title = prompt.title ?: ""
        val message = prompt.message ?: ""

        val onConfirmPositiveButton: (Boolean) -> Unit = {
            geckoResult.complete(prompt.confirm(PromptDelegate.ButtonPrompt.Type.POSITIVE))
        }
        val onConfirmNegativeButton: (Boolean) -> Unit = {
            geckoResult.complete(prompt.confirm(PromptDelegate.ButtonPrompt.Type.NEGATIVE))
        }

        val onDismiss: (Boolean) -> Unit = { geckoResult.complete(prompt.dismiss()) }

        geckoEngineSession.notifyObservers {
            onPromptRequest(
                PromptRequest.Confirm(
                    title,
                    message,
                    false,
                    "",
                    "",
                    "",
                    onConfirmPositiveButton,
                    onConfirmNegativeButton,
                    onDismiss
                ) {
                    onDismiss(false)
                }
            )
        }
        return geckoResult
    }

    private fun GeckoChoice.toChoice(): Choice {
        val choiceChildren = items?.map { it.toChoice() }?.toTypedArray()
        // On the GeckoView docs states that label is a @NonNull, but on run-time
        // we are getting null values
        @Suppress("USELESS_ELVIS")
        return Choice(id, !disabled, label ?: "", selected, separator, choiceChildren)
    }

    /**
     * Convert an array of [GeckoChoice] to Choice array.
     * @return array of Choice
     */
    private fun convertToChoices(
        geckoChoices: Array<out GeckoChoice>
    ): Array<Choice> {

        return geckoChoices.map { geckoChoice ->
            val choice = geckoChoice.toChoice()
            choice
        }.toTypedArray()
    }

    @Suppress("LongParameterList")
    private fun notifyDatePromptRequest(
        title: String,
        initialDateString: String,
        minDateString: String?,
        maxDateString: String?,
        onClear: () -> Unit,
        format: String,
        onConfirm: (String) -> Unit
    ) {
        val initialDate = initialDateString.toDate(format)
        val minDate = if (minDateString.isNullOrEmpty()) null else minDateString.toDate()
        val maxDate = if (maxDateString.isNullOrEmpty()) null else maxDateString.toDate()
        val onSelect: (Date) -> Unit = {
            val stringDate = it.toString(format)
            onConfirm(stringDate)
        }

        val selectionType = when (format) {
            "HH:mm" -> PromptRequest.TimeSelection.Type.TIME
            "yyyy-MM" -> PromptRequest.TimeSelection.Type.MONTH
            "yyyy-MM-dd'T'HH:mm" -> PromptRequest.TimeSelection.Type.DATE_AND_TIME
            else -> PromptRequest.TimeSelection.Type.DATE
        }

        geckoEngineSession.notifyObservers {
            onPromptRequest(
                PromptRequest.TimeSelection(
                    title,
                    initialDate,
                    minDate,
                    maxDate,
                    selectionType,
                    onSelect,
                    onClear
                )
            )
        }
    }

    private fun GeckoAuthOptions.toACLevel(): AC_AUTH_LEVEL {
        return when (level) {
            GECKO_AUTH_LEVEL.NONE -> AC_AUTH_LEVEL.NONE
            GECKO_AUTH_LEVEL.PW_ENCRYPTED -> AC_AUTH_LEVEL.PASSWORD_ENCRYPTED
            GECKO_AUTH_LEVEL.SECURE -> AC_AUTH_LEVEL.SECURED
            else -> {
                AC_AUTH_LEVEL.NONE
            }
        }
    }

    private operator fun Int.contains(mask: Int): Boolean {
        return (this and mask) != 0
    }

    private fun Uri.toFileUri(context: Context): Uri {
        val contentResolver = context.contentResolver
        val cacheUploadDirectory = java.io.File(context.cacheDir, "/uploads")

        if (!cacheUploadDirectory.exists()) {
            cacheUploadDirectory.mkdir()
        }

        val temporalFile = java.io.File(cacheUploadDirectory, getFileName(contentResolver))
        try {
            contentResolver.openInputStream(this)!!.use { inStream ->
                FileOutputStream(temporalFile).use { outStream ->
                    inStream.copyTo(outStream)
                }
            }
        } catch (e: IOException) {
            Logger("GeckoPromptDelegate").warn("Could not convert uri to file uri", e)
        }
        return Uri.parse("file:///${temporalFile.absolutePath}")
    }

    private fun Uri.getFileName(contentResolver: ContentResolver): String {
        val returnUri = this
        var fileName = ""
        contentResolver.query(returnUri, null, null, null, null)?.use { cursor ->
            val nameIndex = cursor.getColumnIndex(OpenableColumns.DISPLAY_NAME)
            cursor.moveToFirst()
            fileName = cursor.getString(nameIndex)
        }
        return fileName
    }
}

@VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
internal fun Array<Choice>.toIdsArray(): Array<String> {
    return this.map { it.id }.toTypedArray()
}

@VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
internal fun Date.toString(format: String): String {
    val formatter = SimpleDateFormat(format, Locale.ROOT)
    return formatter.format(this) ?: ""
}
