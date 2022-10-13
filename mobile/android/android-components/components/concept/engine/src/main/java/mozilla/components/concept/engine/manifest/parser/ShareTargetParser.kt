/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.engine.manifest.parser

import mozilla.components.concept.engine.manifest.WebAppManifest.ShareTarget
import mozilla.components.support.ktx.android.org.json.asSequence
import mozilla.components.support.ktx.android.org.json.toJSONArray
import mozilla.components.support.ktx.android.org.json.tryGetString
import org.json.JSONArray
import org.json.JSONObject
import java.util.Locale

internal object ShareTargetParser {

    /**
     * Parses a share target inside a web app manifest.
     */
    fun parse(json: JSONObject?): ShareTarget? {
        val action = json?.tryGetString("action") ?: return null
        val method = parseMethod(json.tryGetString("method"))
        val encType = parseEncType(json.tryGetString("enctype"))
        val params = json.optJSONObject("params")

        return if (method != null && encType != null && validMethodAndEncType(method, encType)) {
            return ShareTarget(
                action = action,
                method = method,
                encType = encType,
                params = ShareTarget.Params(
                    title = params?.tryGetString("title"),
                    text = params?.tryGetString("text"),
                    url = params?.tryGetString("url"),
                    files = parseFiles(params),
                ),
            )
        } else {
            null
        }
    }

    /**
     * Serializes a share target to JSON for a web app manifest.
     */
    fun serialize(shareTarget: ShareTarget?): JSONObject? {
        shareTarget ?: return null
        return JSONObject().apply {
            put("action", shareTarget.action)
            put("method", shareTarget.method.name)
            put("enctype", shareTarget.encType.type)

            val params = JSONObject().apply {
                put("title", shareTarget.params.title)
                put("text", shareTarget.params.text)
                put("url", shareTarget.params.url)
                put(
                    "files",
                    shareTarget.params.files.asSequence()
                        .map { file ->
                            JSONObject().apply {
                                put("name", file.name)
                                putOpt("accept", file.accept.toJSONArray())
                            }
                        }
                        .asIterable()
                        .toJSONArray(),
                )
            }
            put("params", params)
        }
    }

    /**
     * Convert string to [ShareTarget.RequestMethod]. Returns null if the string is invalid.
     */
    private fun parseMethod(method: String?): ShareTarget.RequestMethod? {
        method ?: return ShareTarget.RequestMethod.GET
        return try {
            ShareTarget.RequestMethod.valueOf(method.uppercase(Locale.ROOT))
        } catch (e: IllegalArgumentException) {
            null
        }
    }

    /**
     * Convert string to [ShareTarget.EncodingType]. Returns null if the string is invalid.
     */
    private fun parseEncType(encType: String?): ShareTarget.EncodingType? {
        val typeString = encType?.lowercase(Locale.ROOT) ?: return ShareTarget.EncodingType.URL_ENCODED
        return ShareTarget.EncodingType.values().find { it.type == typeString }
    }

    /**
     * Checks that [encType] is URL_ENCODED (if [method] is GET or POST) or MULTIPART (only if POST)
     */
    private fun validMethodAndEncType(
        method: ShareTarget.RequestMethod,
        encType: ShareTarget.EncodingType,
    ) = when (encType) {
        ShareTarget.EncodingType.URL_ENCODED -> true
        ShareTarget.EncodingType.MULTIPART -> method == ShareTarget.RequestMethod.POST
    }

    private fun parseFiles(params: JSONObject?) =
        when (val files = params?.opt("files")) {
            is JSONObject -> listOfNotNull(parseFile(files))
            is JSONArray -> files.asSequence { i -> getJSONObject(i) }
                .mapNotNull(::parseFile)
                .toList()
            else -> emptyList()
        }

    private fun parseFile(file: JSONObject): ShareTarget.Files? {
        val name = file.tryGetString("name")
        val accept = file.opt("accept")

        if (name.isNullOrEmpty()) return null

        return ShareTarget.Files(
            name = name,
            accept = when (accept) {
                is String -> listOf(accept)
                is JSONArray -> accept.asSequence { i -> getString(i) }.toList()
                else -> emptyList()
            },
        )
    }
}
