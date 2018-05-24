/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.fretboard

import org.json.JSONArray
import org.json.JSONObject

/**
 * Default JSON parsing implementation
 */
class JSONExperimentParser {
    /**
     * Creates an experiment from its json representation
     *
     * @param jsonObject experiment json object
     * @return created experiment
     */
    fun fromJson(jsonObject: JSONObject): Experiment {
        val bucketsObject: JSONObject? = jsonObject.optJSONObject(BUCKETS_KEY)
        val matchObject: JSONObject? = jsonObject.optJSONObject(MATCH_KEY)
        val regions: List<String>? = matchObject?.optJSONArray(REGIONS_KEY)?.toList()
        val matcher = if (matchObject != null) {
            Experiment.Matcher(matchObject.tryGetString(LANG_KEY), matchObject.tryGetString(APP_ID_KEY), regions)
        } else null
        val bucket = if (bucketsObject != null) {
            Experiment.Bucket(bucketsObject.tryGetInt(MAX_KEY), bucketsObject.tryGetInt(MIN_KEY))
        } else null
        return Experiment(jsonObject.getString(ID_KEY),
            jsonObject.tryGetString(NAME_KEY),
            jsonObject.tryGetString(DESCRIPTION_KEY),
            matcher,
            bucket,
            jsonObject.tryGetLong(LAST_MODIFIED_KEY))
    }

    /**
     * Converts the specified experiment to json
     *
     * @param experiment experiment to convert
     * @return json representation of the experiment
     */
    fun toJson(experiment: Experiment): JSONObject {
        val jsonObject = JSONObject()
        jsonObject.putIfNotNull(NAME_KEY, experiment.name)
        val matchObject = JSONObject()
        matchObject.putIfNotNull(LANG_KEY, experiment.match?.language)
        matchObject.putIfNotNull(APP_ID_KEY, experiment.match?.appId)
        matchObject.putIfNotNull(REGIONS_KEY, experiment.match?.regions?.toJsonArray())
        jsonObject.put(MATCH_KEY, matchObject)
        val bucketsObject = JSONObject()
        bucketsObject.putIfNotNull(MAX_KEY, experiment.bucket?.max)
        bucketsObject.putIfNotNull(MIN_KEY, experiment.bucket?.min)
        jsonObject.put(BUCKETS_KEY, bucketsObject)
        jsonObject.putIfNotNull(DESCRIPTION_KEY, experiment.description)
        jsonObject.put(ID_KEY, experiment.id)
        jsonObject.putIfNotNull(LAST_MODIFIED_KEY, experiment.lastModified)
        return jsonObject
    }

    private fun <T> List<T>.toJsonArray(): JSONArray {
        return fold(JSONArray()) { jsonArray, element -> jsonArray.put(element) }
    }

    @Suppress("UNCHECKED_CAST")
    private fun <T> JSONArray?.toList(): List<T> {
        if (this != null) {
            val result = ArrayList<T>()
            for (i in 0 until length())
                result.add(get(i) as T)
            return result
        }
        return listOf()
    }

    private fun JSONObject.tryGetString(key: String): String? {
        if (!isNull(key)) {
            return getString(key)
        }
        return null
    }

    private fun JSONObject.tryGetInt(key: String): Int? {
        if (!isNull(key)) {
            return getInt(key)
        }
        return null
    }

    private fun JSONObject.tryGetLong(key: String): Long? {
        if (!isNull(key)) {
            return getLong(key)
        }
        return null
    }

    private fun JSONObject.putIfNotNull(key: String, value: Any?) {
        if (value != null) {
            put(key, value)
        }
    }

    companion object {
        private const val BUCKETS_KEY = "buckets"
        private const val MATCH_KEY = "match"
        private const val REGIONS_KEY = "regions"
        private const val LANG_KEY = "lang"
        private const val APP_ID_KEY = "appId"
        private const val MAX_KEY = "max"
        private const val MIN_KEY = "min"
        private const val ID_KEY = "id"
        private const val NAME_KEY = "name"
        private const val DESCRIPTION_KEY = "description"
        private const val LAST_MODIFIED_KEY = "last_modified"
    }
}
