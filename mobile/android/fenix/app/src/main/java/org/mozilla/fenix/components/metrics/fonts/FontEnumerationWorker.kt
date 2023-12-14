/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.metrics.fonts

import android.content.Context
import android.content.res.Configuration
import android.graphics.fonts.Font
import android.graphics.fonts.SystemFonts
import android.os.Build
import android.os.LocaleList
import androidx.work.BackoffPolicy
import androidx.work.CoroutineWorker
import androidx.work.ExistingWorkPolicy
import androidx.work.OneTimeWorkRequest
import androidx.work.WorkManager
import androidx.work.WorkerParameters
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import org.json.JSONArray
import org.json.JSONException
import org.json.JSONObject
import org.mozilla.fenix.Config
import org.mozilla.fenix.GleanMetrics.Metrics
import org.mozilla.fenix.GleanMetrics.Pings
import org.mozilla.fenix.ext.settings
import java.io.File
import java.util.Locale
import java.util.concurrent.TimeUnit
import kotlin.time.Duration.Companion.hours

/**
 * Parse all of the fonts on the user's phone, then put them into the
 * `font_list_json` Metric to be submitted via Telemetry later.
 */
class FontEnumerationWorker(
    context: Context,
    workerParameters: WorkerParameters,
) : CoroutineWorker(context, workerParameters) {
    @Suppress("TooGenericExceptionCaught")
    override suspend fun doWork(): Result = withContext(Dispatchers.IO) {
        val s: String
        try {
            readAllFonts()
            s = createJSONString()
        } catch (e: Exception) {
            return@withContext Result.retry()
        }

        Metrics.fontListJson.set(s)
        Pings.fontList.submit()

        // To avoid getting multiple submissions from new installs, set directly
        // to the desired number of submissions
        applicationContext.settings().numFontListSent = kDesiredSubmissions

        return@withContext Result.success()
    }

    private val brokenFonts: ArrayList<Pair<String, String>> = ArrayList()
    private val fonts: MutableSet<FontMetric> = HashSet()

    @Suppress("TooGenericExceptionCaught")
    private fun readAllFonts() {
        for (path in getSystemFonts()) {
            try {
                fonts.add(FontParser.parse(path))
            } catch (e: Exception) {
                brokenFonts.add(Pair(path, FontParser.calculateFileHash(path)))
            }
        }
        for (path in getAPIFonts()) {
            try {
                fonts.add(FontParser.parse(path))
            } catch (e: Exception) {
                brokenFonts.add(Pair(path, FontParser.calculateFileHash(path)))
            }
        }
    }

    /**
     * This function creates a single JSON String containing
     * The user's phone information, as well as all the fonts and their information,
     * And the names of files that encountered a parsing error.
     */
    @Throws(JSONException::class)
    fun createJSONString(): String {
        val submission = JSONObject()

        run {
            submission.put("submission", kDesiredSubmissions)
            submission.put("brand", Build.BRAND)
            submission.put("device", Build.DEVICE)
            submission.put("hardware", Build.HARDWARE)
            submission.put("manufacturer", Build.MANUFACTURER)
            submission.put("model", Build.MODEL)
            submission.put("product", Build.PRODUCT)
            submission.put("release_version", Build.VERSION.RELEASE)
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
                submission.put("security_patch", Build.VERSION.SECURITY_PATCH)
                submission.put("base_os", Build.VERSION.BASE_OS)
            } else {
                submission.put("security_patch", "too-low-version")
                submission.put("base_os", "too-low-version")
            }
            val config: Configuration = this.applicationContext.resources.configuration
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
                val supportedLocales: LocaleList = LocaleList.getDefault()
                val sb = StringBuilder()
                for (i in 0 until supportedLocales.size()) {
                    val locale: Locale = supportedLocales.get(i)
                    sb.append(locale.toString())
                    sb.append(",")
                }
                submission.put("current_locale", config.locales[0].toString())
                submission.put("all_locales", sb.toString())
            } else {
                @Suppress("DEPRECATION")
                submission.put("current_locale", config.locale.toString())
                submission.put("all_locales", "too-low-version")
            }
        }

        val fontArr = JSONArray()
        for (fontDetails in fonts) {
            fontArr.put(fontDetails.toJson())
        }

        val errorArr = JSONArray()
        for (error in brokenFonts) {
            val errorObj = JSONObject()
            errorObj.put("path", error.first)
            errorObj.put("hash", error.second)
            errorArr.put(errorObj)
        }

        submission.put("fonts", fontArr)
        submission.put("errors", errorArr)

        return submission.toString()
    }

    companion object {
        private const val FONT_ENUMERATOR_WORK_NAME = "org.mozilla.fenix.metrics.font.work"
        private val HOUR_MILLIS: Long = 1.hours.inWholeMilliseconds
        private const val SIX: Long = 6

        /**
         * Schedules the Activated User event if needed.
         */
        fun sendActivatedSignalIfNeeded(context: Context) {
            val instanceWorkManager = WorkManager.getInstance(context)

            if (!Config.channel.isNightlyOrDebug) {
                return
            }

            if (context.settings().numFontListSent >= kDesiredSubmissions) {
                return
            }

            val fontEnumeratorWork =
                OneTimeWorkRequest.Builder(FontEnumerationWorker::class.java)
                    .setInitialDelay(HOUR_MILLIS, TimeUnit.MILLISECONDS)
                    .setBackoffCriteria(BackoffPolicy.EXPONENTIAL, SIX, TimeUnit.HOURS)
                    .build()

            instanceWorkManager.beginUniqueWork(
                FONT_ENUMERATOR_WORK_NAME,
                ExistingWorkPolicy.KEEP,
                fontEnumeratorWork,
            ).enqueue()
        }

        private fun getSystemFonts(): ArrayList<String> {
            val file = File("/system/fonts")
            val ff: Array<out File>? = file.listFiles()
            val systemFonts: ArrayList<String> = ArrayList()
            if (ff != null) {
                for (f in ff) {
                    systemFonts.add(f.absolutePath)
                }
            }
            return systemFonts
        }

        private fun getAPIFonts(): List<String> {
            val aPIFonts: List<String>
            if (Build.VERSION.SDK_INT < Build.VERSION_CODES.Q) {
                aPIFonts = emptyList()
            } else {
                aPIFonts = ArrayList()
                val apiFonts: Set<Font> = SystemFonts.getAvailableFonts()
                for (f in apiFonts) {
                    f.file?.let {
                        aPIFonts.add(it.absolutePath)
                    }
                }
            }
            return aPIFonts
        }

        /**
         * The number of font submissions we would like from a user.
         * We will increment this number by one (via a code patch) when
         * we wish to perform another data collection effort on the Nightly
         * population.
         */
        const val kDesiredSubmissions: Int = 4
    }
}
