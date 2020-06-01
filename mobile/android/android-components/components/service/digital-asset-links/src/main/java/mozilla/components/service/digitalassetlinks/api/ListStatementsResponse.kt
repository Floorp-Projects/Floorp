/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.digitalassetlinks.api

import mozilla.components.service.digitalassetlinks.Statement
import mozilla.components.service.digitalassetlinks.parseStatementListJson
import org.json.JSONObject

/**
 * @property statements A list of all the matching statements that have been found.
 * @property maxAge From serving time, how much longer the response should be considered valid barring further updates.
 * Formatted as a duration in seconds with up to nine fractional digits, terminated by 's'. Example: "3.5s".
 * @property debug Human-readable message containing information about the response.
 */
data class ListStatementsResponse(
    val statements: List<Statement>,
    val maxAge: String,
    val debug: String
)

fun parseListStatementsJson(json: JSONObject): ListStatementsResponse {
    val statements = parseStatementListJson(json.getJSONArray("statements"))
    return ListStatementsResponse(
        statements = statements.mapNotNull { it as? Statement },
        maxAge = json.getString("maxAge"),
        debug = json.optString("debugString")
    )
}
