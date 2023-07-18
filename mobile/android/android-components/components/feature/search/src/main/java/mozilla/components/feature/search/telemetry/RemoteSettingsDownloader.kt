/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.search.telemetry

import org.json.JSONObject

/**
 * Remote server config for SERP Telemetry
 */
class RemoteSettingsDownloader {

    /**
     * Download the remote server config.
     */
    @Suppress("ComplexMethod")
    fun telemetryJson() = JSONObject(
        """{
    "data": [
        { "schema": 1683504016013, "taggedCodes": ["ffab", "ffcm", "ffhp", "ffip", "ffit", "ffnt", "ffocus", "ffos", "ffsb", "fpas", "fpsa", "ftas", "ftsa", "lm", "newext"], 
        "telemetryId": "duckduckgo", "organicCodes": [], "codeParamName": "t", "queryParamName": "q", "searchPageRegexp": "^https://duckduckgo\\.com/", "expectedOrganicCodes": ["hz", "h_", "hs", "ha", "hb", "hc", "hg", "hh", "hi", "hj"], 
        "extraAdServersRegexps": ["^https://duckduckgo.com/y\\.js?.*ad_provider\\=", "^https://www\\.amazon\\.(?:[a-z.]{2,24}).*(?:tag=duckduckgo-)"], 
        "id": "9dfd626b-26f2-4913-9d0a-27db6cb7d8ca", "last_modified": 1683640711097 },
        { "schema": 1678791413786, "taggedCodes": ["firefox-a", "firefox-b", "firefox-b-1", "firefox-b-ab", "firefox-b-1-ab", "firefox-b-d", "firefox-b-1-d", "firefox-b-e", "firefox-b-1-e", "firefox-b-m", "firefox-b-1-m", 
        "firefox-b-o", "firefox-b-1-o", "firefox-b-lm", "firefox-b-1-lm", "firefox-b-lg", "firefox-b-huawei-h1611", "firefox-b-is-oem1", "firefox-b-oem1", "firefox-b-oem2", "firefox-b-tinno", "firefox-b-pn-wt", "firefox-b-pn-wt-us", "ubuntu", "ubuntu-sn"], 
        "telemetryId": "google", "organicCodes": [], "codeParamName": "client", "queryParamName": "q", "searchPageRegexp": "^https://www\\.google\\.(?:.+)/search", "adServerAttributes": ["rw"], "followOnParamNames": ["oq", "ved", "ei"], 
        "extraAdServersRegexps": ["^https?://www\\.google(?:adservices)?\\.com/(?:pagead/)?aclk"], 
        "id": "635a3325-1995-42d6-be09-dbe4b2a95453", "last_modified": 1678922429086 },
        { "schema": 1671479978127, "taggedCodes": ["mzl", "813cf1dd", "16eeffc4"], 
        "telemetryId": "ecosia", "organicCodes": [], "codeParamName": "tt", "queryParamName": "q", 
        "searchPageRegexp": "^https://www\\.ecosia\\.org/", "filter_expression": "env.version|versionCompare(\"110.0a1\")>=0", "expectedOrganicCodes": [], 
        "extraAdServersRegexps": ["^https://www\\.bing\\.com/acli?c?k"], 
        "id": "9a487171-3a06-4647-8866-36250ec84f3a", "last_modified": 1671565418576 },
        { "schema": 1671122205131, "taggedCodes": ["MOZ2", "MOZ4", "MOZ5", "MOZA", "MOZB", "MOZD", "MOZE", "MOZI", "MOZL", "MOZM", "MOZO", "MOZR", "MOZT", "MOZW", "MOZX", "MOZSL01", "MOZSL02", "MOZSL03"], 
        "telemetryId": "bing", "organicCodes": [], "codeParamName": "pc", "queryParamName": "q", "followOnCookies": [{ "host": "www.bing.com", "name": "SRCHS", "codeParamName": "PC", "extraCodePrefixes": ["QBRE"], "extraCodeParamName": "form" }], 
        "searchPageRegexp": "^https://www\\.bing\\.com/search", "extraAdServersRegexps": ["^https://www\\.bing\\.com/acli?c?k"], "id": "e1eec461-f1f3-40de-b94b-3b670b78108c", "last_modified": 1671479978088 },
        { "schema": 1643107838909, "taggedCodes": ["monline_dg", "monline_3_dg", "monline_4_dg", "monline_7_dg"], 
        "telemetryId": "baidu", "organicCodes": [], "codeParamName": "tn", "queryParamName": "wd", 
        "searchPageRegexp": "^https://www\\.baidu\\.com/(?:s|baidu)", "followOnParamNames": ["oq"], 
        "extraAdServersRegexps": ["^https?://www\\.baidu\\.com/baidu\\.php?"], 
        "id": "19c434a3-d173-4871-9743-290ac92a3f6a", "last_modified": 1643136933989 }
    ]
}""",
    )
}
