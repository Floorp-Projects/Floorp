/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.digitalassetlinks.api

import mozilla.components.service.digitalassetlinks.AssetDescriptor
import mozilla.components.service.digitalassetlinks.Relation
import mozilla.components.service.digitalassetlinks.Statement
import mozilla.components.support.ktx.android.org.json.asSequence
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
    val debug: String,
)

internal fun parseListStatementsJson(json: JSONObject): ListStatementsResponse {
    val statements = json.getJSONArray("statements")
        .asSequence { i -> getJSONObject(i) }
        .mapNotNull { statementJson ->
            val relationString = statementJson.getString("relation")
            val relation = Relation.values().find { relationString == it.kindAndDetail }

            val targetJson = statementJson.getJSONObject("target")
            val webJson = targetJson.optJSONObject("web")
            val androidJson = targetJson.optJSONObject("androidApp")
            val target = when {
                webJson != null -> AssetDescriptor.Web(site = webJson.getString("site"))
                androidJson != null -> AssetDescriptor.Android(
                    packageName = androidJson.getString("packageName"),
                    sha256CertFingerprint = androidJson.getJSONObject("certificate")
                        .getString("sha256Fingerprint"),
                )
                else -> null
            }

            if (relation != null && target != null) {
                Statement(relation, target)
            } else {
                null
            }
        }
    return ListStatementsResponse(
        statements = statements.toList(),
        maxAge = json.getString("maxAge"),
        debug = json.optString("debugString"),
    )
}
