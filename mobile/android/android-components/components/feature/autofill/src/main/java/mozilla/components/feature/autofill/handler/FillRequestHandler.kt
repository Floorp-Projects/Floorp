/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.autofill.handler

import android.annotation.SuppressLint
import android.app.assist.AssistStructure
import android.content.Context
import android.os.Build
import android.service.autofill.FillRequest
import android.service.autofill.FillResponse
import androidx.annotation.RequiresApi
import mozilla.components.feature.autofill.AutofillConfiguration
import mozilla.components.feature.autofill.facts.emitAutofillRequestFact
import mozilla.components.feature.autofill.response.dataset.DatasetBuilder
import mozilla.components.feature.autofill.response.dataset.LoginDatasetBuilder
import mozilla.components.feature.autofill.response.fill.AuthFillResponseBuilder
import mozilla.components.feature.autofill.response.fill.FillResponseBuilder
import mozilla.components.feature.autofill.response.fill.LoginFillResponseBuilder
import mozilla.components.feature.autofill.structure.ParsedStructure
import mozilla.components.feature.autofill.structure.RawStructure
import mozilla.components.feature.autofill.structure.getLookupDomain
import mozilla.components.feature.autofill.structure.parseStructure

internal const val EXTRA_LOGIN_ID = "loginId"
// Maximum number of logins we are going to display in the autofill overlay.
internal const val MAX_LOGINS = 10

/**
 * Class responsible for handling [FillRequest]s and returning [FillResponse]s.
 */
@RequiresApi(Build.VERSION_CODES.O)
internal class FillRequestHandler(
    private val context: Context,
    private val configuration: AutofillConfiguration
) {
    /**
     * Handles a fill request for the given [AssistStructure] and returns a matching [FillResponse]
     * or `null` if the request could not be handled or the passed in [AssistStructure] is `null`.
     */
    @SuppressLint("InlinedApi")
    @Suppress("ReturnCount")
    suspend fun handle(
        structure: RawStructure?,
        forceUnlock: Boolean = false
    ): FillResponseBuilder? {
        if (structure == null) {
            return null
        }

        val parsedStructure = parseStructure(context, structure) ?: return null
        return handle(parsedStructure, forceUnlock)
    }

    suspend fun handle(
        parsedStructure: ParsedStructure,
        forceUnlock: Boolean = false
    ): FillResponseBuilder {
        val lookupDomain = parsedStructure.getLookupDomain(configuration.publicSuffixList)
        val needsConfirmation = !configuration.verifier.hasCredentialRelationship(
            context,
            lookupDomain,
            parsedStructure.packageName
        )

        val logins = configuration.storage
            .getByBaseDomain(lookupDomain)
            .take(MAX_LOGINS)

        return if (!configuration.lock.keepUnlocked() && !forceUnlock) {
            AuthFillResponseBuilder(parsedStructure)
        } else {
            emitAutofillRequestFact(hasLogins = logins.isNotEmpty(), needsConfirmation)
            LoginFillResponseBuilder(parsedStructure, logins, needsConfirmation)
        }
    }

    /**
     * Handles a fill request for the given [RawStructure] and returns only a [DatasetBuilder] for
     * the given [loginId] -  or `null` if the request could not be handled or the passed in
     * [RawStructure] is `null`
     */
    @Suppress("ReturnCount")
    suspend fun handleConfirmation(structure: RawStructure?, loginId: String): DatasetBuilder? {
        if (structure == null) {
            return null
        }

        val parsedStructure = parseStructure(context, structure) ?: return null
        val lookupDomain = parsedStructure.getLookupDomain(configuration.publicSuffixList)

        val logins = configuration.storage.getByBaseDomain(lookupDomain)
        if (logins.isEmpty()) {
            return null
        }

        val login = logins.firstOrNull { login -> login.guid == loginId } ?: return null

        return LoginDatasetBuilder(parsedStructure, login, needsConfirmation = false)
    }
}
