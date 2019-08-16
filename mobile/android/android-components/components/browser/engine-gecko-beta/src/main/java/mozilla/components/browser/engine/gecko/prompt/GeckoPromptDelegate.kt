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
import mozilla.components.concept.engine.prompt.PromptRequest.Alert
import mozilla.components.concept.engine.prompt.PromptRequest.Authentication.Level
import mozilla.components.concept.engine.prompt.PromptRequest.Authentication.Method
import mozilla.components.concept.engine.prompt.PromptRequest.MenuChoice
import mozilla.components.concept.engine.prompt.PromptRequest.MultipleChoice
import mozilla.components.concept.engine.prompt.PromptRequest.SingleChoice
import mozilla.components.concept.engine.prompt.PromptRequest.TimeSelection
import mozilla.components.support.base.log.logger.Logger
import mozilla.components.support.ktx.kotlin.toDate
import org.mozilla.geckoview.AllowOrDeny
import org.mozilla.geckoview.GeckoResult
import org.mozilla.geckoview.GeckoSession
import org.mozilla.geckoview.GeckoSession.PromptDelegate.AlertCallback
import org.mozilla.geckoview.GeckoSession.PromptDelegate.AuthCallback
import org.mozilla.geckoview.GeckoSession.PromptDelegate.AuthOptions
import org.mozilla.geckoview.GeckoSession.PromptDelegate.AuthOptions.AUTH_FLAG_CROSS_ORIGIN_SUB_RESOURCE
import org.mozilla.geckoview.GeckoSession.PromptDelegate.AuthOptions.AUTH_FLAG_HOST
import org.mozilla.geckoview.GeckoSession.PromptDelegate.AuthOptions.AUTH_FLAG_ONLY_PASSWORD
import org.mozilla.geckoview.GeckoSession.PromptDelegate.AuthOptions.AUTH_FLAG_PREVIOUS_FAILED
import org.mozilla.geckoview.GeckoSession.PromptDelegate.AuthOptions.AUTH_LEVEL_NONE
import org.mozilla.geckoview.GeckoSession.PromptDelegate.AuthOptions.AUTH_LEVEL_PW_ENCRYPTED
import org.mozilla.geckoview.GeckoSession.PromptDelegate.AuthOptions.AUTH_LEVEL_SECURE
import org.mozilla.geckoview.GeckoSession.PromptDelegate.BUTTON_TYPE_NEGATIVE
import org.mozilla.geckoview.GeckoSession.PromptDelegate.BUTTON_TYPE_NEUTRAL
import org.mozilla.geckoview.GeckoSession.PromptDelegate.BUTTON_TYPE_POSITIVE
import org.mozilla.geckoview.GeckoSession.PromptDelegate.ButtonCallback
import org.mozilla.geckoview.GeckoSession.PromptDelegate.Choice.CHOICE_TYPE_MENU
import org.mozilla.geckoview.GeckoSession.PromptDelegate.Choice.CHOICE_TYPE_MULTIPLE
import org.mozilla.geckoview.GeckoSession.PromptDelegate.Choice.CHOICE_TYPE_SINGLE
import org.mozilla.geckoview.GeckoSession.PromptDelegate.ChoiceCallback
import org.mozilla.geckoview.GeckoSession.PromptDelegate.DATETIME_TYPE_DATE
import org.mozilla.geckoview.GeckoSession.PromptDelegate.DATETIME_TYPE_DATETIME_LOCAL
import org.mozilla.geckoview.GeckoSession.PromptDelegate.DATETIME_TYPE_MONTH
import org.mozilla.geckoview.GeckoSession.PromptDelegate.DATETIME_TYPE_TIME
import org.mozilla.geckoview.GeckoSession.PromptDelegate.DATETIME_TYPE_WEEK
import org.mozilla.geckoview.GeckoSession.PromptDelegate.FileCallback
import org.mozilla.geckoview.GeckoSession.PromptDelegate.TextCallback
import java.io.FileOutputStream
import java.io.IOException
import java.security.InvalidParameterException
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

typealias GeckoChoice = GeckoSession.PromptDelegate.Choice

/**
 * Gecko-based PromptDelegate implementation.
 */
@Suppress("TooManyFunctions")
internal class GeckoPromptDelegate(private val geckoEngineSession: GeckoEngineSession) :
    GeckoSession.PromptDelegate {

    override fun onChoicePrompt(
        session: GeckoSession,
        title: String?,
        msg: String?,
        type: Int,
        geckoChoices: Array<out GeckoChoice>,
        callback: ChoiceCallback
    ) {
        val choices = convertToChoices(geckoChoices)
        val onConfirmSingleChoice: (Choice) -> Unit = { selectedChoice ->
            callback.confirm(selectedChoice.id)
        }
        val onConfirmMultipleSelection: (Array<Choice>) -> Unit = { selectedChoices ->
            val ids = selectedChoices.toIdsArray()
            callback.confirm(ids)
        }

        val promptRequest = when (type) {
            CHOICE_TYPE_SINGLE -> SingleChoice(choices, onConfirmSingleChoice)
            CHOICE_TYPE_MENU -> MenuChoice(choices, onConfirmSingleChoice)
            CHOICE_TYPE_MULTIPLE -> MultipleChoice(choices, onConfirmMultipleSelection)
            else -> throw InvalidParameterException("$type is not a valid Gecko @Choice.ChoiceType")
        }

        geckoEngineSession.notifyObservers {
            onPromptRequest(promptRequest)
        }
    }

    override fun onAlert(
        session: GeckoSession,
        title: String?,
        message: String?,
        callback: AlertCallback
    ) {

        val hasShownManyDialogs = callback.hasCheckbox()
        val onDismiss: () -> Unit = {
            callback.dismiss()
        }

        geckoEngineSession.notifyObservers {
            onPromptRequest(Alert(title ?: "", message ?: "", hasShownManyDialogs, onDismiss) { showMoreDialogs ->
                callback.checkboxValue = showMoreDialogs
                callback.dismiss()
            })
        }
    }

    override fun onFilePrompt(
        session: GeckoSession,
        title: String?,
        selectionType: Int,
        mimeTypes: Array<out String>?,
        callback: FileCallback
    ) {

        val onSelectMultiple: (Context, Array<Uri>) -> Unit = { context, uris ->
            val filesUris = uris.map {
                it.toFileUri(context)
            }.toTypedArray()

            callback.confirm(context, filesUris)
        }

        val isMultipleFilesSelection = selectionType == GeckoSession.PromptDelegate.FILE_TYPE_MULTIPLE

        val captureMode = PromptRequest.File.FacingMode.NONE

        val onSelectSingle: (Context, Uri) -> Unit = { context, uri ->
            callback.confirm(context, uri.toFileUri(context))
        }

        val onDismiss: () -> Unit = {
            callback.dismiss()
        }

        geckoEngineSession.notifyObservers {
            onPromptRequest(
                PromptRequest.File(
                    mimeTypes ?: emptyArray(),
                    isMultipleFilesSelection,
                    captureMode,
                    onSelectSingle,
                    onSelectMultiple,
                    onDismiss
                )
            )
        }
    }

    override fun onDateTimePrompt(
        session: GeckoSession,
        title: String?,
        type: Int,
        value: String?,
        minDate: String?,
        maxDate: String?,
        geckoCallback: TextCallback
    ) {
        val initialDateString = value ?: ""
        val onClear: () -> Unit = {
            geckoCallback.confirm("")
        }
        val format = when (type) {
            DATETIME_TYPE_DATE -> "yyyy-MM-dd"
            DATETIME_TYPE_MONTH -> "yyyy-MM"
            DATETIME_TYPE_WEEK -> "yyyy-'W'ww"
            DATETIME_TYPE_TIME -> "HH:mm"
            DATETIME_TYPE_DATETIME_LOCAL -> "yyyy-MM-dd'T'HH:mm"
            else -> {
                throw InvalidParameterException("$type is not a valid DatetimeType")
            }
        }

        notifyDatePromptRequest(
            title ?: "",
            initialDateString,
            minDate,
            maxDate,
            onClear,
            format,
            geckoCallback
        )
    }

    override fun onAuthPrompt(
        session: GeckoSession,
        title: String?,
        message: String?,
        options: AuthOptions,
        geckoCallback: AuthCallback
    ) {

        val flags = options.flags
        val userName = options.username ?: ""
        val password = options.password ?: ""
        val method = if (flags in AUTH_FLAG_HOST) Method.HOST else Method.PROXY

        val level = getAuthLevel(options)

        val onlyShowPassword = flags in AUTH_FLAG_ONLY_PASSWORD
        val previousFailed = flags in AUTH_FLAG_PREVIOUS_FAILED
        val isCrossOrigin = flags in AUTH_FLAG_CROSS_ORIGIN_SUB_RESOURCE

        val onConfirm: (String, String) -> Unit = { user, pass ->
            if (onlyShowPassword) {
                geckoCallback.confirm(pass)
            } else {
                geckoCallback.confirm(user, pass)
            }
        }

        val onDismiss: () -> Unit = {
            geckoCallback.dismiss()
        }

        geckoEngineSession.notifyObservers {
            onPromptRequest(
                PromptRequest.Authentication(
                    title ?: "",
                    message ?: "",
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
    }

    override fun onTextPrompt(
        session: GeckoSession,
        title: String?,
        inputLabel: String?,
        inputValue: String?,
        callback: TextCallback
    ) {
        val hasShownManyDialogs = callback.hasCheckbox()
        val onDismiss: () -> Unit = {
            callback.dismiss()
        }

        geckoEngineSession.notifyObservers {
            onPromptRequest(
                PromptRequest.TextPrompt(
                    title ?: "",
                    inputLabel ?: "",
                    inputValue ?: "",
                    hasShownManyDialogs,
                    onDismiss
                ) { showMoreDialogs, valueInput ->
                    callback.checkboxValue = showMoreDialogs
                    callback.confirm(valueInput)
                })
        }
    }

    override fun onColorPrompt(
        session: GeckoSession,
        title: String?,
        defaultColor: String?,
        callback: TextCallback
    ) {

        val onConfirm: (String) -> Unit = {
            callback.confirm(it)
        }
        val onDismiss: () -> Unit = {
            callback.dismiss()
        }
        geckoEngineSession.notifyObservers {
            onPromptRequest(
                PromptRequest.Color(defaultColor ?: "", onConfirm, onDismiss)
            )
        }
    }

    override fun onPopupRequest(session: GeckoSession, targetUri: String?): GeckoResult<AllowOrDeny> {
        val geckoResult = GeckoResult<AllowOrDeny>()
        val onAllow: () -> Unit = { geckoResult.complete(AllowOrDeny.ALLOW) }
        val onDeny: () -> Unit = { geckoResult.complete(AllowOrDeny.DENY) }

        geckoEngineSession.notifyObservers {
            onPromptRequest(
                PromptRequest.Popup(targetUri ?: "", onAllow, onDeny)
            )
        }
        return geckoResult
    }

    override fun onButtonPrompt(
        session: GeckoSession,
        title: String?,
        message: String?,
        buttonTitles: Array<out String?>?,
        callback: ButtonCallback
    ) {
        val hasShownManyDialogs = callback.hasCheckbox()
        val positiveButtonTitle = buttonTitles?.get(BUTTON_TYPE_POSITIVE) ?: ""
        val negativeButtonTitle = buttonTitles?.get(BUTTON_TYPE_NEGATIVE) ?: ""
        val neutralButtonTitle = buttonTitles?.get(BUTTON_TYPE_NEUTRAL) ?: ""

        val onConfirmPositiveButton: (Boolean) -> Unit = { showMoreDialogs ->
            callback.checkboxValue = showMoreDialogs
            callback.confirm(BUTTON_TYPE_POSITIVE)
        }

        val onConfirmNegativeButton: (Boolean) -> Unit = { showMoreDialogs ->
            callback.checkboxValue = showMoreDialogs
            callback.confirm(BUTTON_TYPE_NEGATIVE)
        }

        val onConfirmNeutralButton: (Boolean) -> Unit = { showMoreDialogs ->
            callback.checkboxValue = showMoreDialogs
            callback.confirm(BUTTON_TYPE_NEUTRAL)
        }

        val onDismiss: () -> Unit = {
            callback.dismiss()
        }

        geckoEngineSession.notifyObservers {
            onPromptRequest(
                PromptRequest.Confirm(
                    title ?: "",
                    message ?: "",
                    hasShownManyDialogs,
                    positiveButtonTitle,
                    negativeButtonTitle,
                    neutralButtonTitle,
                    onConfirmPositiveButton,
                    onConfirmNegativeButton,
                    onConfirmNeutralButton,
                    onDismiss
                )
            )
        }
    }

    private fun GeckoChoice.toChoice(): Choice {
        val choiceChildren = items?.map { it.toChoice() }?.toTypedArray()
        return Choice(id, !disabled, label, selected, separator, choiceChildren)
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
        geckoCallback: TextCallback
    ) {
        val initialDate = initialDateString.toDate(format)
        val minDate = if (minDateString.isNullOrEmpty()) null else minDateString.toDate()
        val maxDate = if (maxDateString.isNullOrEmpty()) null else maxDateString.toDate()
        val onSelect: (Date) -> Unit = {
            val stringDate = it.toString(format)
            geckoCallback.confirm(stringDate)
        }

        val selectionType = when (format) {
            "HH:mm" -> TimeSelection.Type.TIME
            "yyyy-MM" -> TimeSelection.Type.MONTH
            "yyyy-MM-dd'T'HH:mm" -> TimeSelection.Type.DATE_AND_TIME
            else -> TimeSelection.Type.DATE
        }

        geckoEngineSession.notifyObservers {
            onPromptRequest(TimeSelection(title, initialDate, minDate, maxDate, selectionType, onSelect, onClear))
        }
    }

    private fun getAuthLevel(options: AuthOptions): Level {
        return when (options.level) {
            AUTH_LEVEL_NONE -> Level.NONE
            AUTH_LEVEL_PW_ENCRYPTED -> Level.PASSWORD_ENCRYPTED
            AUTH_LEVEL_SECURE -> Level.SECURED
            else -> {
                Level.NONE
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
