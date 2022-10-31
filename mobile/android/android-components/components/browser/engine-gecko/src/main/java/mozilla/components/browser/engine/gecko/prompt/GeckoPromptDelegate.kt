/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.gecko.prompt

import android.content.Context
import android.net.Uri
import androidx.annotation.VisibleForTesting
import mozilla.components.browser.engine.gecko.GeckoEngineSession
import mozilla.components.browser.engine.gecko.ext.convertToChoices
import mozilla.components.browser.engine.gecko.ext.toAddress
import mozilla.components.browser.engine.gecko.ext.toAutocompleteAddress
import mozilla.components.browser.engine.gecko.ext.toAutocompleteCreditCard
import mozilla.components.browser.engine.gecko.ext.toCreditCardEntry
import mozilla.components.browser.engine.gecko.ext.toLoginEntry
import mozilla.components.concept.engine.prompt.Choice
import mozilla.components.concept.engine.prompt.PromptRequest
import mozilla.components.concept.engine.prompt.PromptRequest.MenuChoice
import mozilla.components.concept.engine.prompt.PromptRequest.MultipleChoice
import mozilla.components.concept.engine.prompt.PromptRequest.SingleChoice
import mozilla.components.concept.engine.prompt.ShareData
import mozilla.components.concept.storage.Address
import mozilla.components.concept.storage.CreditCardEntry
import mozilla.components.concept.storage.Login
import mozilla.components.concept.storage.LoginEntry
import mozilla.components.support.base.log.logger.Logger
import mozilla.components.support.ktx.android.net.getFileName
import mozilla.components.support.ktx.kotlin.toDate
import mozilla.components.support.utils.TimePicker.shouldShowMillisecondsPicker
import mozilla.components.support.utils.TimePicker.shouldShowSecondsPicker
import org.mozilla.geckoview.AllowOrDeny
import org.mozilla.geckoview.Autocomplete
import org.mozilla.geckoview.GeckoResult
import org.mozilla.geckoview.GeckoSession
import org.mozilla.geckoview.GeckoSession.PromptDelegate
import org.mozilla.geckoview.GeckoSession.PromptDelegate.AutocompleteRequest
import org.mozilla.geckoview.GeckoSession.PromptDelegate.BeforeUnloadPrompt
import org.mozilla.geckoview.GeckoSession.PromptDelegate.DateTimePrompt.Type.DATE
import org.mozilla.geckoview.GeckoSession.PromptDelegate.DateTimePrompt.Type.DATETIME_LOCAL
import org.mozilla.geckoview.GeckoSession.PromptDelegate.DateTimePrompt.Type.MONTH
import org.mozilla.geckoview.GeckoSession.PromptDelegate.DateTimePrompt.Type.TIME
import org.mozilla.geckoview.GeckoSession.PromptDelegate.DateTimePrompt.Type.WEEK
import org.mozilla.geckoview.GeckoSession.PromptDelegate.PromptResponse
import java.io.File
import java.io.FileOutputStream
import java.io.IOException
import java.io.InputStream
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
typealias GECKO_PROMPT_SHARE_RESULT = PromptDelegate.SharePrompt.Result
typealias AC_AUTH_LEVEL = PromptRequest.Authentication.Level
typealias AC_AUTH_METHOD = PromptRequest.Authentication.Method
typealias AC_FILE_FACING_MODE = PromptRequest.File.FacingMode

/**
 * Gecko-based PromptDelegate implementation.
 */
@Suppress("LargeClass")
internal class GeckoPromptDelegate(private val geckoEngineSession: GeckoEngineSession) :
    PromptDelegate {

    override fun onCreditCardSave(
        session: GeckoSession,
        request: AutocompleteRequest<Autocomplete.CreditCardSaveOption>,
    ): GeckoResult<PromptResponse> {
        val geckoResult = GeckoResult<PromptResponse>()

        val onConfirm: (CreditCardEntry) -> Unit = { creditCard ->
            if (!request.isComplete) {
                geckoResult.complete(
                    request.confirm(
                        Autocomplete.CreditCardSelectOption(creditCard.toAutocompleteCreditCard()),
                    ),
                )
            }
        }

        val onDismiss: () -> Unit = {
            request.dismissSafely(geckoResult)
        }

        geckoEngineSession.notifyObservers {
            onPromptRequest(
                PromptRequest.SaveCreditCard(
                    creditCard = request.options[0].value.toCreditCardEntry(),
                    onConfirm = onConfirm,
                    onDismiss = onDismiss,
                ).also {
                    request.delegate = PromptInstanceDismissDelegate(
                        geckoEngineSession,
                        it,
                    )
                },
            )
        }

        return geckoResult
    }

    /**
     * Handle a credit card selection prompt request. This is triggered by the user
     * focusing on a credit card input field.
     *
     * @param session The [GeckoSession] that triggered the request.
     * @param request The [AutocompleteRequest] containing the credit card selection request.
     */
    override fun onCreditCardSelect(
        session: GeckoSession,
        request: AutocompleteRequest<Autocomplete.CreditCardSelectOption>,
    ): GeckoResult<PromptResponse>? {
        val geckoResult = GeckoResult<PromptResponse>()

        val onConfirm: (CreditCardEntry) -> Unit = { creditCard ->
            if (!request.isComplete) {
                geckoResult.complete(
                    request.confirm(
                        Autocomplete.CreditCardSelectOption(creditCard.toAutocompleteCreditCard()),
                    ),
                )
            }
        }

        val onDismiss: () -> Unit = {
            request.dismissSafely(geckoResult)
        }

        geckoEngineSession.notifyObservers {
            onPromptRequest(
                PromptRequest.SelectCreditCard(
                    creditCards = request.options.map { it.value.toCreditCardEntry() },
                    onDismiss = onDismiss,
                    onConfirm = onConfirm,
                ),
            )
        }

        return geckoResult
    }

    override fun onLoginSave(
        session: GeckoSession,
        prompt: AutocompleteRequest<Autocomplete.LoginSaveOption>,
    ): GeckoResult<PromptResponse>? {
        val geckoResult = GeckoResult<PromptResponse>()
        val onConfirmSave: (LoginEntry) -> Unit = { entry ->
            if (!prompt.isComplete) {
                geckoResult.complete(prompt.confirm(Autocomplete.LoginSelectOption(entry.toLoginEntry())))
            }
        }
        val onDismiss: () -> Unit = {
            prompt.dismissSafely(geckoResult)
        }

        geckoEngineSession.notifyObservers {
            onPromptRequest(
                PromptRequest.SaveLoginPrompt(
                    hint = prompt.options[0].hint,
                    logins = prompt.options.map { it.value.toLoginEntry() },
                    onConfirm = onConfirmSave,
                    onDismiss = onDismiss,
                ).also {
                    prompt.delegate = PromptInstanceDismissDelegate(
                        geckoEngineSession,
                        it,
                    )
                },
            )
        }
        return geckoResult
    }

    override fun onLoginSelect(
        session: GeckoSession,
        prompt: AutocompleteRequest<Autocomplete.LoginSelectOption>,
    ): GeckoResult<PromptResponse>? {
        val geckoResult = GeckoResult<PromptResponse>()
        val onConfirmSelect: (Login) -> Unit = { login ->
            if (!prompt.isComplete) {
                geckoResult.complete(prompt.confirm(Autocomplete.LoginSelectOption(login.toLoginEntry())))
            }
        }
        val onDismiss: () -> Unit = {
            prompt.dismissSafely(geckoResult)
        }

        // `guid` plus exactly one of `httpRealm` and `formSubmitURL` must be present to be a valid login entry.
        val loginList = prompt.options.filter { option ->
            option.value.guid != null && (option.value.formActionOrigin != null || option.value.httpRealm != null)
        }.map { option ->
            Login(
                guid = option.value.guid!!,
                origin = option.value.origin,
                formActionOrigin = option.value.formActionOrigin,
                httpRealm = option.value.httpRealm,
                username = option.value.username,
                password = option.value.password,
            )
        }

        geckoEngineSession.notifyObservers {
            onPromptRequest(
                PromptRequest.SelectLoginPrompt(
                    logins = loginList,
                    onConfirm = onConfirmSelect,
                    onDismiss = onDismiss,
                ),
            )
        }
        return geckoResult
    }

    override fun onChoicePrompt(
        session: GeckoSession,
        geckoPrompt: PromptDelegate.ChoicePrompt,
    ): GeckoResult<PromptResponse>? {
        val geckoResult = GeckoResult<PromptResponse>()
        val choices = convertToChoices(geckoPrompt.choices)

        val onDismiss: () -> Unit = {
            geckoPrompt.dismissSafely(geckoResult)
        }

        val onConfirmSingleChoice: (Choice) -> Unit = { selectedChoice ->
            if (!geckoPrompt.isComplete) {
                geckoResult.complete(geckoPrompt.confirm(selectedChoice.id))
            }
        }
        val onConfirmMultipleSelection: (Array<Choice>) -> Unit = { selectedChoices ->
            if (!geckoPrompt.isComplete) {
                val ids = selectedChoices.toIdsArray()
                geckoResult.complete(geckoPrompt.confirm(ids))
            }
        }

        val promptRequest = when (geckoPrompt.type) {
            GECKO_PROMPT_CHOICE_TYPE.SINGLE -> SingleChoice(
                choices,
                onConfirmSingleChoice,
                onDismiss,
            )
            GECKO_PROMPT_CHOICE_TYPE.MENU -> MenuChoice(
                choices,
                onConfirmSingleChoice,
                onDismiss,
            )
            GECKO_PROMPT_CHOICE_TYPE.MULTIPLE -> MultipleChoice(
                choices,
                onConfirmMultipleSelection,
                onDismiss,
            )
            else -> throw InvalidParameterException("${geckoPrompt.type} is not a valid Gecko @Choice.ChoiceType")
        }

        geckoPrompt.delegate = ChoicePromptDelegate(
            geckoEngineSession,
            promptRequest,
        )

        geckoEngineSession.notifyObservers {
            onPromptRequest(promptRequest)
        }

        return geckoResult
    }

    override fun onAddressSelect(
        session: GeckoSession,
        request: AutocompleteRequest<Autocomplete.AddressSelectOption>,
    ): GeckoResult<PromptResponse> {
        val geckoResult = GeckoResult<PromptResponse>()

        val onConfirm: (Address) -> Unit = { address ->
            if (!request.isComplete) {
                geckoResult.complete(
                    request.confirm(
                        Autocomplete.AddressSelectOption(address.toAutocompleteAddress()),
                    ),
                )
            }
        }

        val onDismiss: () -> Unit = {
            request.dismissSafely(geckoResult)
        }

        geckoEngineSession.notifyObservers {
            onPromptRequest(
                PromptRequest.SelectAddress(
                    addresses = request.options.map { it.value.toAddress() },
                    onConfirm = onConfirm,
                    onDismiss = onDismiss,
                ),
            )
        }

        return geckoResult
    }

    override fun onAlertPrompt(
        session: GeckoSession,
        prompt: PromptDelegate.AlertPrompt,
    ): GeckoResult<PromptResponse> {
        val geckoResult = GeckoResult<PromptResponse>()
        val onDismiss: () -> Unit = { prompt.dismissSafely(geckoResult) }
        val onConfirm: (Boolean) -> Unit = { _ -> onDismiss() }
        val title = prompt.title ?: ""
        val message = prompt.message ?: ""

        geckoEngineSession.notifyObservers {
            onPromptRequest(
                PromptRequest.Alert(
                    title,
                    message,
                    false,
                    onConfirm,
                    onDismiss,
                ),
            )
        }
        return geckoResult
    }

    override fun onFilePrompt(
        session: GeckoSession,
        prompt: PromptDelegate.FilePrompt,
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
            if (!prompt.isComplete) {
                geckoResult.complete(prompt.confirm(context, filesUris))
            }
        }

        val onSelectSingle: (Context, Uri) -> Unit = { context, uri ->
            if (!prompt.isComplete) {
                geckoResult.complete(prompt.confirm(context, uri.toFileUri(context)))
            }
        }

        val onDismiss: () -> Unit = {
            prompt.dismissSafely(geckoResult)
        }

        geckoEngineSession.notifyObservers {
            onPromptRequest(
                PromptRequest.File(
                    prompt.mimeTypes ?: emptyArray(),
                    isMultipleFilesSelection,
                    captureMode,
                    onSelectSingle,
                    onSelectMultiple,
                    onDismiss,
                ),
            )
        }
        return geckoResult
    }

    @Suppress("ComplexMethod")
    override fun onDateTimePrompt(
        session: GeckoSession,
        prompt: PromptDelegate.DateTimePrompt,
    ): GeckoResult<PromptResponse>? {
        val geckoResult = GeckoResult<PromptResponse>()
        val onConfirm: (String) -> Unit = {
            if (!prompt.isComplete) {
                geckoResult.complete(prompt.confirm(it))
            }
        }

        val onDismiss: () -> Unit = {
            prompt.dismissSafely(geckoResult)
        }

        val onClear: () -> Unit = {
            onConfirm("")
        }
        val initialDateString = prompt.defaultValue ?: ""
        val stepValue = if (prompt.stepValue.isNullOrBlank()) {
            null
        } else {
            prompt.stepValue
        }

        val format = when (prompt.type) {
            DATE -> "yyyy-MM-dd"
            MONTH -> "yyyy-MM"
            WEEK -> "yyyy-'W'ww"
            TIME -> {
                if (shouldShowMillisecondsPicker(stepValue?.toFloat())) {
                    "HH:mm:ss.SSS"
                } else if (shouldShowSecondsPicker(stepValue?.toFloat())) {
                    "HH:mm:ss"
                } else {
                    "HH:mm"
                }
            }
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
            stepValue,
            onClear,
            format,
            onConfirm,
            onDismiss,
        )

        return geckoResult
    }

    override fun onAuthPrompt(
        session: GeckoSession,
        geckoPrompt: PromptDelegate.AuthPrompt,
    ): GeckoResult<PromptResponse>? {
        val geckoResult = GeckoResult<PromptResponse>()
        val title = geckoPrompt.title ?: ""
        val message = geckoPrompt.message ?: ""
        val uri = geckoPrompt.authOptions.uri
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
                if (!geckoPrompt.isComplete) {
                    if (onlyShowPassword) {
                        geckoResult.complete(geckoPrompt.confirm(pass))
                    } else {
                        geckoResult.complete(geckoPrompt.confirm(user, pass))
                    }
                }
            }

        val onDismiss: () -> Unit = { geckoPrompt.dismissSafely(geckoResult) }

        geckoEngineSession.notifyObservers {
            onPromptRequest(
                PromptRequest.Authentication(
                    uri,
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
                    onDismiss,
                ),
            )
        }
        return geckoResult
    }

    override fun onTextPrompt(
        session: GeckoSession,
        prompt: PromptDelegate.TextPrompt,
    ): GeckoResult<PromptResponse>? {
        val geckoResult = GeckoResult<PromptResponse>()
        val title = prompt.title ?: ""
        val inputLabel = prompt.message ?: ""
        val inputValue = prompt.defaultValue ?: ""
        val onDismiss: () -> Unit = { prompt.dismissSafely(geckoResult) }
        val onConfirm: (Boolean, String) -> Unit = { _, valueInput ->
            if (!prompt.isComplete) {
                geckoResult.complete(prompt.confirm(valueInput))
            }
        }

        geckoEngineSession.notifyObservers {
            onPromptRequest(
                PromptRequest.TextPrompt(
                    title,
                    inputLabel,
                    inputValue,
                    false,
                    onConfirm,
                    onDismiss,
                ),
            )
        }

        return geckoResult
    }

    override fun onColorPrompt(
        session: GeckoSession,
        prompt: PromptDelegate.ColorPrompt,
    ): GeckoResult<PromptResponse>? {
        val geckoResult = GeckoResult<PromptResponse>()
        val onConfirm: (String) -> Unit = {
            if (!prompt.isComplete) {
                geckoResult.complete(prompt.confirm(it))
            }
        }
        val onDismiss: () -> Unit = { prompt.dismissSafely(geckoResult) }

        val defaultColor = prompt.defaultValue ?: ""

        geckoEngineSession.notifyObservers {
            onPromptRequest(
                PromptRequest.Color(defaultColor, onConfirm, onDismiss),
            )
        }
        return geckoResult
    }

    override fun onPopupPrompt(
        session: GeckoSession,
        prompt: PromptDelegate.PopupPrompt,
    ): GeckoResult<PromptResponse> {
        val geckoResult = GeckoResult<PromptResponse>()
        val onAllow: () -> Unit = {
            if (!prompt.isComplete) {
                geckoResult.complete(prompt.confirm(AllowOrDeny.ALLOW))
            }
        }
        val onDeny: () -> Unit = {
            if (!prompt.isComplete) {
                geckoResult.complete(prompt.confirm(AllowOrDeny.DENY))
            }
        }

        geckoEngineSession.notifyObservers {
            onPromptRequest(
                PromptRequest.Popup(prompt.targetUri ?: "", onAllow, onDeny),
            )
        }
        return geckoResult
    }

    override fun onBeforeUnloadPrompt(
        session: GeckoSession,
        geckoPrompt: BeforeUnloadPrompt,
    ): GeckoResult<PromptResponse>? {
        val geckoResult = GeckoResult<PromptResponse>()
        val title = geckoPrompt.title ?: ""
        val onAllow: () -> Unit = {
            if (!geckoPrompt.isComplete) {
                geckoResult.complete(geckoPrompt.confirm(AllowOrDeny.ALLOW))
            }
        }
        val onDeny: () -> Unit = {
            if (!geckoPrompt.isComplete) {
                geckoResult.complete(geckoPrompt.confirm(AllowOrDeny.DENY))
                geckoEngineSession.notifyObservers { onBeforeUnloadPromptDenied() }
            }
        }

        geckoEngineSession.notifyObservers {
            onPromptRequest(PromptRequest.BeforeUnload(title, onAllow, onDeny))
        }

        return geckoResult
    }

    override fun onSharePrompt(
        session: GeckoSession,
        prompt: PromptDelegate.SharePrompt,
    ): GeckoResult<PromptResponse> {
        val geckoResult = GeckoResult<PromptResponse>()
        val onSuccess = {
            if (!prompt.isComplete) {
                geckoResult.complete(prompt.confirm(GECKO_PROMPT_SHARE_RESULT.SUCCESS))
            }
        }
        val onFailure = {
            if (!prompt.isComplete) {
                geckoResult.complete(prompt.confirm(GECKO_PROMPT_SHARE_RESULT.FAILURE))
            }
        }
        val onDismiss = { prompt.dismissSafely(geckoResult) }

        geckoEngineSession.notifyObservers {
            onPromptRequest(
                PromptRequest.Share(
                    ShareData(
                        title = prompt.title,
                        text = prompt.text,
                        url = prompt.uri,
                    ),
                    onSuccess,
                    onFailure,
                    onDismiss,
                ),
            )
        }
        return geckoResult
    }

    override fun onButtonPrompt(
        session: GeckoSession,
        prompt: PromptDelegate.ButtonPrompt,
    ): GeckoResult<PromptResponse>? {
        val geckoResult = GeckoResult<PromptResponse>()
        val title = prompt.title ?: ""
        val message = prompt.message ?: ""

        val onConfirmPositiveButton: (Boolean) -> Unit = {
            if (!prompt.isComplete) {
                geckoResult.complete(prompt.confirm(PromptDelegate.ButtonPrompt.Type.POSITIVE))
            }
        }
        val onConfirmNegativeButton: (Boolean) -> Unit = {
            if (!prompt.isComplete) {
                geckoResult.complete(prompt.confirm(PromptDelegate.ButtonPrompt.Type.NEGATIVE))
            }
        }

        val onDismiss: (Boolean) -> Unit = { prompt.dismissSafely(geckoResult) }

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
                    onDismiss,
                ) {
                    onDismiss(false)
                },
            )
        }
        return geckoResult
    }

    override fun onRepostConfirmPrompt(
        session: GeckoSession,
        prompt: PromptDelegate.RepostConfirmPrompt,
    ): GeckoResult<PromptResponse>? {
        val geckoResult = GeckoResult<PromptResponse>()

        val onConfirm: () -> Unit = {
            if (!prompt.isComplete) {
                geckoResult.complete(prompt.confirm(AllowOrDeny.ALLOW))
            }
        }
        val onCancel: () -> Unit = {
            if (!prompt.isComplete) {
                geckoResult.complete(prompt.confirm(AllowOrDeny.DENY))
                geckoEngineSession.notifyObservers { onRepostPromptCancelled() }
            }
        }

        geckoEngineSession.notifyObservers {
            onPromptRequest(
                PromptRequest.Repost(
                    onConfirm,
                    onCancel,
                ),
            )
        }
        return geckoResult
    }

    @Suppress("LongParameterList")
    private fun notifyDatePromptRequest(
        title: String,
        initialDateString: String,
        minDateString: String?,
        maxDateString: String?,
        stepValue: String?,
        onClear: () -> Unit,
        format: String,
        onConfirm: (String) -> Unit,
        onDismiss: () -> Unit,
    ) {
        val initialDate = initialDateString.toDate(format)
        val minDate = if (minDateString.isNullOrEmpty()) null else minDateString.toDate()
        val maxDate = if (maxDateString.isNullOrEmpty()) null else maxDateString.toDate()
        val onSelect: (Date) -> Unit = {
            val stringDate = it.toString(format)
            onConfirm(stringDate)
        }

        val selectionType = when (format) {
            "HH:mm", "HH:mm:ss", "HH:mm:ss.SSS" -> PromptRequest.TimeSelection.Type.TIME
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
                    stepValue,
                    selectionType,
                    onSelect,
                    onClear,
                    onDismiss,
                ),
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
                copyFile(temporalFile, inStream)
            }
        } catch (e: IOException) {
            Logger("GeckoPromptDelegate").warn("Could not convert uri to file uri", e)
        }
        return Uri.parse("file:///${temporalFile.absolutePath}")
    }

    @VisibleForTesting
    internal fun copyFile(temporalFile: File, inStream: InputStream): Long {
        return FileOutputStream(temporalFile).use { outStream ->
            inStream.copyTo(outStream)
        }
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

/**
 * Only dismiss if the prompt is not already dismissed.
 */
@VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
internal fun PromptDelegate.BasePrompt.dismissSafely(geckoResult: GeckoResult<PromptResponse>) {
    if (!this.isComplete) {
        geckoResult.complete(dismiss())
    }
}
