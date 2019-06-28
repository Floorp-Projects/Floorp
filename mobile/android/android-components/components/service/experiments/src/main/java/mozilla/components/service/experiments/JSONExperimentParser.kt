/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.experiments

import mozilla.components.support.ktx.android.org.json.putIfNotNull
import mozilla.components.support.ktx.android.org.json.toList
import mozilla.components.support.ktx.android.org.json.tryGetLong
import mozilla.components.support.ktx.android.org.json.tryGetString
import org.json.JSONArray
import org.json.JSONException
import org.json.JSONObject

/**
 * Default JSON parsing implementation
 */
internal class JSONExperimentParser {
    /**
     * Creates an experiment from its json representation
     *
     * @param jsonObject experiment json object
     * @return created experiment
     */
    fun fromJson(jsonObject: JSONObject): Experiment {
        val bucketsObject: JSONObject = jsonObject.getJSONObject(BUCKETS_KEY)
        val bucket = Experiment.Buckets(
            bucketsObject.getInt(BUCKETS_START_KEY),
            bucketsObject.getInt(BUCKETS_COUNT_KEY)
        )

        val branchesJSONList: List<JSONObject> = jsonObject.getJSONArray(BRANCHES_KEY).toList()
        val branches = branchesJSONList.map {
            Experiment.Branch(
                it.getString(BRANCHES_NAME_KEY),
                it.getInt(BRANCHES_RATIO_KEY)
            )
        }
        if (branches.isEmpty()) {
            throw JSONException("Branches array should not be empty")
        }

        val matchObject: JSONObject = jsonObject.getJSONObject(MATCH_KEY)
        val regions: List<String>? = matchObject.optJSONArray(MATCH_REGIONS_KEY)?.toList()
        val debugTags: List<String>? = matchObject.optJSONArray(MATCH_DEBUG_TAGS_KEY)?.toList()
        val matcher = Experiment.Matcher(
                appId = matchObject.tryGetString(MATCH_APP_ID_KEY),
                appDisplayVersion = matchObject.tryGetString(MATCH_APP_DISPLAY_VERSION_KEY),
                appMinVersion = matchObject.tryGetString(MATCH_APP_MIN_VERSION_KEY),
                appMaxVersion = matchObject.tryGetString(MATCH_APP_MAX_VERSION_KEY),
                localeLanguage = matchObject.tryGetString(MATCH_LOCALE_LANGUAGE_KEY),
                localeCountry = matchObject.tryGetString(MATCH_LOCALE_COUNTRY_KEY),
                deviceManufacturer = matchObject.tryGetString(MATCH_DEVICE_MANUFACTURER_KEY),
                deviceModel = matchObject.tryGetString(MATCH_DEVICE_MODEL_KEY),
                regions = regions,
                debugTags = debugTags
        )

        return Experiment(
            id = jsonObject.getString(ID_KEY),
            description = jsonObject.getString(DESCRIPTION_KEY),
            lastModified = jsonObject.tryGetLong(LAST_MODIFIED_KEY),
            schemaModified = jsonObject.tryGetLong(SCHEMA_KEY),
            buckets = bucket,
            branches = branches,
            match = matcher
        )
    }

    /**
     * Converts the specified experiment to json
     *
     * @param experiment experiment to convert
     *
     * @return json representation of the experiment
     */
    fun toJson(experiment: Experiment): JSONObject {
        val jsonObject = JSONObject()

        jsonObject.put(ID_KEY, experiment.id)
        jsonObject.put(DESCRIPTION_KEY, experiment.description)
        jsonObject.putIfNotNull(LAST_MODIFIED_KEY, experiment.lastModified)
        jsonObject.putIfNotNull(SCHEMA_KEY, experiment.schemaModified)

        val bucketsObject = JSONObject()
        bucketsObject.put(BUCKETS_START_KEY, experiment.buckets.start)
        bucketsObject.put(BUCKETS_COUNT_KEY, experiment.buckets.count)
        jsonObject.put(BUCKETS_KEY, bucketsObject)

        val branchesList = JSONArray()
        experiment.branches.forEach {
            val branchObject = JSONObject()
            branchObject.put(BRANCHES_NAME_KEY, it.name)
            branchObject.put(BRANCHES_RATIO_KEY, it.ratio)
            branchesList.put(branchObject)
        }
        jsonObject.put(BRANCHES_KEY, branchesList)

        val matchObject = matchersToJson(experiment)
        jsonObject.put(MATCH_KEY, matchObject)

        return jsonObject
    }

    private fun matchersToJson(experiment: Experiment): JSONObject {
        val matchObject = JSONObject()
        matchObject.putIfNotNull(MATCH_APP_ID_KEY, experiment.match.appId)
        matchObject.putIfNotNull(MATCH_APP_DISPLAY_VERSION_KEY, experiment.match.appDisplayVersion)
        matchObject.putIfNotNull(MATCH_APP_MIN_VERSION_KEY, experiment.match.appMinVersion)
        matchObject.putIfNotNull(MATCH_APP_MAX_VERSION_KEY, experiment.match.appMaxVersion)
        matchObject.putIfNotNull(MATCH_LOCALE_COUNTRY_KEY, experiment.match.localeCountry)
        matchObject.putIfNotNull(MATCH_LOCALE_LANGUAGE_KEY, experiment.match.localeLanguage)
        matchObject.putIfNotNull(MATCH_DEVICE_MANUFACTURER_KEY, experiment.match.deviceManufacturer)
        matchObject.putIfNotNull(MATCH_DEVICE_MODEL_KEY, experiment.match.deviceModel)
        matchObject.putIfNotNull(MATCH_REGIONS_KEY, experiment.match.regions?.let { JSONArray(it) })
        matchObject.putIfNotNull(MATCH_DEBUG_TAGS_KEY, experiment.match.debugTags?.let { JSONArray(it) })
        return matchObject
    }

    companion object {
        private const val ID_KEY = "id"
        private const val DESCRIPTION_KEY = "description"
        private const val LAST_MODIFIED_KEY = "last_modified"
        private const val SCHEMA_KEY = "schema"

        private const val BUCKETS_KEY = "buckets"
        private const val BUCKETS_START_KEY = "start"
        private const val BUCKETS_COUNT_KEY = "count"

        private const val BRANCHES_KEY = "branches"
        private const val BRANCHES_NAME_KEY = "name"
        private const val BRANCHES_RATIO_KEY = "ratio"

        private const val MATCH_KEY = "match"
        private const val MATCH_APP_ID_KEY = "app_id"
        private const val MATCH_APP_DISPLAY_VERSION_KEY = "app_display_version"
        private const val MATCH_APP_MIN_VERSION_KEY = "app_min_version"
        private const val MATCH_APP_MAX_VERSION_KEY = "app_max_version"
        private const val MATCH_LOCALE_COUNTRY_KEY = "locale_country"
        private const val MATCH_LOCALE_LANGUAGE_KEY = "locale_language"
        private const val MATCH_DEVICE_MANUFACTURER_KEY = "device_manufacturer"
        private const val MATCH_DEVICE_MODEL_KEY = "device_model"
        private const val MATCH_REGIONS_KEY = "regions"
        private const val MATCH_DEBUG_TAGS_KEY = "debug_tags"
    }
}
