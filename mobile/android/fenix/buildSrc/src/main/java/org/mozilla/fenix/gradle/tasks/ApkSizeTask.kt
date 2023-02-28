/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.gradle.tasks

import org.gradle.api.DefaultTask
import org.gradle.api.tasks.Input
import org.gradle.api.tasks.TaskAction
import org.json.JSONArray
import org.json.JSONException
import org.json.JSONObject
import java.io.File
import java.nio.file.Files
import java.nio.file.Paths

/**
 * Gradle task for determining the size of APKs and logging them in a perfherder compatible format.
 */
open class ApkSizeTask : DefaultTask() {
    /**
     * Name of the build variant getting built.
     */
    @Input
    var variantName: String? = null

    /**
     * List of APKs that get build for the build variant.
     */
    @Input
    var apks: List<String>? = null

    @TaskAction
    fun logApkSize() {
        val apkSizes = determineApkSizes()
        if (apkSizes.isEmpty()) {
            println("Couldn't determine APK sizes for perfherder")
            return
        }

        val json = buildPerfherderJson(apkSizes) ?: return

        println("PERFHERDER_DATA: $json")
    }

    private fun determineApkSizes(): Map<String, Long> {
        val variantOutputPath = variantName?.removePrefix("fenix")?.toLowerCase()
        val basePath = listOf(
            "${project.projectDir}", "build", "outputs", "apk", "fenix", variantOutputPath
        ).joinToString(File.separator)

        return requireNotNull(apks).associateWith { apk ->
            val rawPath = "$basePath${File.separator}$apk"

            try {
                val path = Paths.get(rawPath)
                Files.size(path)
            } catch (t: Throwable) {
                println("Could not determine size of $apk ($rawPath)")
                t.printStackTrace()
                0
            }
        }.filter { (_, size) -> size > 0 }
    }

    /**
     * Returns perfherder compatible JSON for tracking the file size of APKs.
     *
     * ```
     * {
     *   "framework": {
     *     "name": "build_metrics"
     *   },
     *   "suites": [
     *     {
     *       "name": "apk-size-[debug,nightly,beta,release]",
     *       "lowerIsBetter": true,
     *       "subtests": [
     *         { "name": "app-arm64-v8a-debug.apk", "value": 98855735 },
     *         { "name": "app-armeabi-v7a-debug.apk", "value": 92300031 },
     *         { "name": "app-x86-debug.apk", "value": 103410909 },
     *         { "name": "app-x86_64-debug.apk", "value": 102465675 }
     *       ],
     *       "value":98855735,
     *       "shouldAlert":false
     *     }
     *   ]
     * }
     * ```
     */
    private fun buildPerfherderJson(apkSize: Map<String, Long>): JSONObject? {
        return try {
            val data = JSONObject()

            val framework = JSONObject()
            framework.put("name", "build_metrics")
            data.put("framework", framework)

            val suites = JSONArray()

            val suite = JSONObject()
            suite.put("name", "apk-size-$variantName")
            suite.put("value", getSummarySize(apkSize))
            suite.put("lowerIsBetter", true)
            suite.put("shouldAlert", false)

            val subtests = JSONArray()
            apkSize.forEach { (apk, size) ->
                val subtest = JSONObject()
                subtest.put("name", apk)
                subtest.put("value", size)
                subtests.put(subtest)
            }
            suite.put("subtests", subtests)

            suites.put(suite)

            data.put("suites", suites)

            data
        } catch (e: JSONException) {
            println("Couldn't generate perfherder JSON")
            e.printStackTrace()
            null
        }
    }
}

/**
 * Returns a summarized size for the APKs. This is the main value that is getting tracked. The size
 * of the individual APKs will be reported as "subtests".
 */
private fun getSummarySize(apkSize: Map<String, Long>): Long {
    val arm64size = apkSize.keys.find { it.contains("arm64") }?.let { apk -> apkSize[apk] }
    if (arm64size != null) {
        // If available we will report the size of the arm64 APK as the summary. This is the most
        // important and most installed APK.
        return arm64size
    }

    // If there's no arm64 APK then we calculate a simple average.
    return apkSize.values.sum() / apkSize.size
}
