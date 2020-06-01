/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.digitalassetlinks

import mozilla.components.support.ktx.android.org.json.asSequence
import org.json.JSONArray
import org.json.JSONObject

/**
 * Represents a statement that can be found in an assetlinks.json file.
 */
sealed class StatementResult

/**
 * Entry in a Digital Asset Links statement file.
 */
data class Statement(
    val relation: List<Relation>,
    val target: AssetDescriptor
) : StatementResult()

/**
 * Include statements point to another Digital Asset Links statement file.
 */
data class IncludeStatement(
    val include: String
) : StatementResult()

internal fun parseStatementJson(json: JSONObject): Sequence<StatementResult> {
    val include = json.optString("include")
    if (include.isNotEmpty()) {
        return sequenceOf(IncludeStatement(include))
    }

    val relationTypes = Relation.values()
    val relations = json.getJSONArray("relation")
        .asSequence { i -> getString(i) }
        .mapNotNull { relation -> relationTypes.find { relation == it.kindAndDetail } }
        .toList()

    if (relations.isEmpty()) return emptySequence()

    val target = json.getJSONObject("target")
    val assets = when (target.getString("namespace")) {
        "web" -> sequenceOf(
            AssetDescriptor.Web(site = target.getString("site"))
        )
        "android_app" -> {
            val packageName = target.getString("package_name")
            target.getJSONArray("sha256_cert_fingerprints")
                .asSequence { i -> getString(i) }
                .map { fingerprint -> AssetDescriptor.Android(packageName, fingerprint) }
        }
        else -> emptySequence()
    }

    return assets.map { asset -> Statement(relations, asset) }
}

internal fun parseStatementListJson(json: JSONArray): List<StatementResult> {
    return json.asSequence { i -> getJSONObject(i) }
        .flatMap { parseStatementJson(it).asSequence() }
        .toList()
}
