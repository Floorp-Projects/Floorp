/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.crash.service

import android.app.ActivityManager
import android.content.Context
import android.content.pm.PackageManager
import android.net.Uri
import android.os.Build
import androidx.annotation.VisibleForTesting
import androidx.core.content.pm.PackageInfoCompat
import mozilla.components.concept.base.crash.Breadcrumb
import mozilla.components.lib.crash.Crash
import mozilla.components.support.base.ext.getStacktraceAsJsonString
import mozilla.components.support.base.ext.getStacktraceAsString
import mozilla.components.support.base.log.logger.Logger
import org.json.JSONArray
import org.json.JSONException
import org.json.JSONObject
import java.io.BufferedReader
import java.io.File
import java.io.FileInputStream
import java.io.FileNotFoundException
import java.io.FileReader
import java.io.IOException
import java.io.InputStreamReader
import java.io.OutputStream
import java.net.HttpURLConnection
import java.net.URL
import java.nio.channels.Channels
import java.util.Locale
import java.util.concurrent.TimeUnit
import java.util.zip.GZIPOutputStream
import kotlin.random.Random
import mozilla.components.Build as AcBuild

/* This ID is used for all Mozilla products.  Setting as default if no ID is passed in */
private const val MOZILLA_PRODUCT_ID = "{eeb82917-e434-4870-8148-5c03d4caa81b}"

@VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
internal const val CAUGHT_EXCEPTION_TYPE = "caught exception"
internal const val UNCAUGHT_EXCEPTION_TYPE = "uncaught exception"
internal const val FATAL_NATIVE_CRASH_TYPE = "fatal native crash"
internal const val NON_FATAL_NATIVE_CRASH_TYPE = "non-fatal native crash"

internal const val DEFAULT_VERSION_NAME = "N/A"
internal const val DEFAULT_VERSION_CODE = "N/A"
internal const val DEFAULT_VERSION = "N/A"
internal const val DEFAULT_BUILD_ID = "N/A"
internal const val DEFAULT_VENDOR = "N/A"
internal const val DEFAULT_RELEASE_CHANNEL = "N/A"
internal const val DEFAULT_DISTRIBUTION_ID = "N/A"

private const val KEY_CRASH_ID = "CrashID"

private const val MINI_DUMP_FILE_EXT = "dmp"
private const val EXTRAS_FILE_EXT = "extra"
private const val FILE_REGEX = "([0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12})\\."

/**
 * A [CrashReporterService] implementation uploading crash reports to crash-stats.mozilla.com.
 *
 * @param applicationContext The application [Context].
 * @param appName A human-readable app name. This name is used on crash-stats.mozilla.com to filter crashes by app.
 *                The name needs to be safelisted for the server to accept the crash.
 *                [File a bug](https://bugzilla.mozilla.org/enter_bug.cgi?product=Socorro) if you would like to get your
 *                app added to the safelist.
 * @param appId The application ID assigned by Socorro server.
 * @param version The engine version.
 * @param buildId The engine build ID.
 * @param vendor The application vendor name.
 * @param serverUrl The URL of the server.
 * @param versionName The version of the application.
 * @param versionCode The version code of the application.
 * @param releaseChannel The release channel of the application.
 * @param distributionId The distribution id of the application.
 */
@Suppress("LargeClass", "LongParameterList")
class MozillaSocorroService(
    private val applicationContext: Context,
    private val appName: String,
    private val appId: String = MOZILLA_PRODUCT_ID,
    private val version: String = DEFAULT_VERSION,
    private val buildId: String = DEFAULT_BUILD_ID,
    private val vendor: String = DEFAULT_VENDOR,
    @get:VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal var serverUrl: String? = null,
    private var versionName: String = DEFAULT_VERSION_NAME,
    private var versionCode: String = DEFAULT_VERSION_CODE,
    private val releaseChannel: String = DEFAULT_RELEASE_CHANNEL,
    private val distributionId: String = DEFAULT_DISTRIBUTION_ID,
) : CrashReporterService {
    private val logger = Logger("mozac/MozillaSocorroCrashHelperService")
    private val startTime = System.currentTimeMillis()
    private val ignoreKeys = hashSetOf("URL", "ServerURL", "StackTraces")

    override val id: String = "socorro"

    override val name: String = "Socorro"

    override fun createCrashReportUrl(identifier: String): String? {
        return "https://crash-stats.mozilla.org/report/index/$identifier"
    }

    init {
        val packageInfo = try {
            applicationContext.packageManager.getPackageInfo(applicationContext.packageName, 0)
        } catch (e: PackageManager.NameNotFoundException) {
            logger.error("package name not found, failed to get application version")
            null
        }

        packageInfo?.let {
            if (versionName == DEFAULT_VERSION_NAME) {
                try {
                    versionName = packageInfo.versionName ?: DEFAULT_VERSION_NAME
                } catch (e: IllegalStateException) {
                    logger.error("failed to get application version")
                }
            }

            if (versionCode == DEFAULT_VERSION_CODE) {
                try {
                    versionCode = PackageInfoCompat.getLongVersionCode(packageInfo).toString()
                } catch (e: IllegalStateException) {
                    logger.error("failed to get application version code")
                }
            }
        }

        if (serverUrl == null) {
            serverUrl = Uri.parse("https://crash-reports.mozilla.com/submit")
                .buildUpon()
                .appendQueryParameter("id", appId)
                .appendQueryParameter("version", versionName)
                .appendQueryParameter("android_component_version", AcBuild.version)
                .build().toString()
        }
    }

    override fun report(crash: Crash.UncaughtExceptionCrash): String? {
        return sendReport(
            crash.timestamp,
            crash.throwable,
            miniDumpFilePath = null,
            extrasFilePath = null,
            isNativeCodeCrash = false,
            isFatalCrash = true,
            breadcrumbs = crash.breadcrumbs,
        )
    }

    override fun report(crash: Crash.NativeCodeCrash): String? {
        return sendReport(
            crash.timestamp,
            throwable = null,
            miniDumpFilePath = crash.minidumpPath,
            extrasFilePath = crash.extrasPath,
            isNativeCodeCrash = true,
            isFatalCrash = crash.isFatal,
            breadcrumbs = crash.breadcrumbs,
        )
    }

    override fun report(throwable: Throwable, breadcrumbs: ArrayList<Breadcrumb>): String? {
        /* Not sending caught exceptions to Socorro */
        return null
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    @Suppress("LongParameterList")
    internal fun sendReport(
        timestamp: Long,
        throwable: Throwable?,
        miniDumpFilePath: String?,
        extrasFilePath: String?,
        isNativeCodeCrash: Boolean,
        isFatalCrash: Boolean,
        breadcrumbs: ArrayList<Breadcrumb>,
    ): String? {
        val url = URL(serverUrl)
        val boundary = generateBoundary()
        var conn: HttpURLConnection? = null

        val breadcrumbsJson = JSONArray()
        for (breadcrumb in breadcrumbs) {
            breadcrumbsJson.put(breadcrumb.toJson())
        }

        try {
            conn = url.openConnection() as HttpURLConnection
            conn.requestMethod = "POST"
            conn.doOutput = true
            conn.setRequestProperty("Content-Type", "multipart/form-data; boundary=$boundary")
            conn.setRequestProperty("Content-Encoding", "gzip")

            sendCrashData(
                conn.outputStream, boundary, timestamp, throwable, miniDumpFilePath, extrasFilePath,
                isNativeCodeCrash, isFatalCrash, breadcrumbsJson.toString(),
            )

            BufferedReader(InputStreamReader(conn.inputStream)).use { reader ->
                val map = parseResponse(reader)

                val id = map?.get(KEY_CRASH_ID)

                if (id != null) {
                    logger.info("Crash reported to Socorro: $id")
                } else {
                    logger.info("Server rejected crash report")
                }

                return id
            }
        } catch (e: IOException) {
            try {
                logger.error("failed to send report to Socorro with " + conn?.responseCode, e)
            } catch (e: IOException) {
                logger.error("failed to send report to Socorro", e)
            }

            return null
        } finally {
            conn?.disconnect()
        }
    }

    private fun parseResponse(reader: BufferedReader): Map<String, String>? {
        val map = mutableMapOf<String, String>()

        reader.readLines().forEach { line ->
            val position = line.indexOf("=")
            if (position != -1) {
                val key = line.substring(0, position)
                val value = unescape(line.substring(position + 1))
                map[key] = value
            }
        }

        return map
    }

    @Suppress("LongParameterList", "LongMethod", "ComplexMethod")
    private fun sendCrashData(
        os: OutputStream,
        boundary: String,
        timestamp: Long,
        throwable: Throwable?,
        miniDumpFilePath: String?,
        extrasFilePath: String?,
        isNativeCodeCrash: Boolean,
        isFatalCrash: Boolean,
        breadcrumbs: String,
    ) {
        val nameSet = mutableSetOf<String>()
        val gzipOs = GZIPOutputStream(os)
        sendPart(gzipOs, boundary, "ProductName", appName, nameSet)
        sendPart(gzipOs, boundary, "ProductID", appId, nameSet)
        sendPart(gzipOs, boundary, "Version", versionName, nameSet)
        sendPart(gzipOs, boundary, "ApplicationBuildID", versionCode, nameSet)
        sendPart(gzipOs, boundary, "AndroidComponentVersion", AcBuild.version, nameSet)
        sendPart(gzipOs, boundary, "GleanVersion", AcBuild.gleanSdkVersion, nameSet)
        sendPart(gzipOs, boundary, "ApplicationServicesVersion", AcBuild.applicationServicesVersion, nameSet)
        sendPart(gzipOs, boundary, "GeckoViewVersion", version, nameSet)
        sendPart(gzipOs, boundary, "BuildID", buildId, nameSet)
        sendPart(gzipOs, boundary, "Vendor", vendor, nameSet)
        sendPart(gzipOs, boundary, "Breadcrumbs", breadcrumbs, nameSet)
        sendPart(gzipOs, boundary, "useragent_locale", Locale.getDefault().toString(), nameSet)
        sendPart(gzipOs, boundary, "DistributionID", distributionId, nameSet)

        extrasFilePath?.let {
            val regex = "$FILE_REGEX$EXTRAS_FILE_EXT".toRegex()
            if (regex.matchEntire(it.substringAfterLast("/")) != null) {
                val extrasFile = File(it)
                val extrasMap = readExtrasFromFile(extrasFile)
                for (key in extrasMap.keys) {
                    sendPart(gzipOs, boundary, key, extrasMap[key], nameSet)
                }
                extrasFile.delete()
            }
        }

        if (throwable?.stackTrace?.isEmpty() == false) {
            sendPart(
                gzipOs,
                boundary,
                "JavaStackTrace",
                getExceptionStackTrace(
                    throwable,
                    !isNativeCodeCrash && !isFatalCrash,
                ),
                nameSet,
            )

            sendPart(gzipOs, boundary, "JavaException", throwable.getStacktraceAsJsonString(), nameSet)
        }

        miniDumpFilePath?.let {
            val regex = "$FILE_REGEX$MINI_DUMP_FILE_EXT".toRegex()
            if (regex.matchEntire(it.substringAfterLast("/")) != null) {
                val minidumpFile = File(it)
                sendFile(gzipOs, boundary, "upload_file_minidump", minidumpFile, nameSet)
                minidumpFile.delete()
            }
        }

        when {
            isNativeCodeCrash && isFatalCrash ->
                sendPart(gzipOs, boundary, "CrashType", FATAL_NATIVE_CRASH_TYPE, nameSet)
            isNativeCodeCrash && !isFatalCrash ->
                sendPart(gzipOs, boundary, "CrashType", NON_FATAL_NATIVE_CRASH_TYPE, nameSet)
            !isNativeCodeCrash && isFatalCrash ->
                sendPart(gzipOs, boundary, "CrashType", UNCAUGHT_EXCEPTION_TYPE, nameSet)
            !isNativeCodeCrash && !isFatalCrash ->
                sendPart(gzipOs, boundary, "CrashType", CAUGHT_EXCEPTION_TYPE, nameSet)
        }

        sendPackageInstallTime(gzipOs, boundary, nameSet)
        sendProcessName(gzipOs, boundary, nameSet)
        sendPart(gzipOs, boundary, "ReleaseChannel", releaseChannel, nameSet)
        sendPart(
            gzipOs,
            boundary,
            "StartupTime",
            TimeUnit.MILLISECONDS.toSeconds(startTime).toString(),
            nameSet,
        )
        sendPart(
            gzipOs,
            boundary,
            "CrashTime",
            TimeUnit.MILLISECONDS.toSeconds(timestamp).toString(),
            nameSet,
        )
        sendPart(gzipOs, boundary, "Android_PackageName", applicationContext.packageName, nameSet)
        sendPart(gzipOs, boundary, "Android_Manufacturer", Build.MANUFACTURER, nameSet)
        sendPart(gzipOs, boundary, "Android_Model", Build.MODEL, nameSet)
        sendPart(gzipOs, boundary, "Android_Board", Build.BOARD, nameSet)
        sendPart(gzipOs, boundary, "Android_Brand", Build.BRAND, nameSet)
        sendPart(gzipOs, boundary, "Android_Device", Build.DEVICE, nameSet)
        sendPart(gzipOs, boundary, "Android_Display", Build.DISPLAY, nameSet)
        sendPart(gzipOs, boundary, "Android_Fingerprint", Build.FINGERPRINT, nameSet)
        sendPart(gzipOs, boundary, "Android_Hardware", Build.HARDWARE, nameSet)
        sendPart(
            gzipOs,
            boundary,
            "Android_Version",
            "${Build.VERSION.SDK_INT} (${Build.VERSION.CODENAME})",
            nameSet,
        )

        if (Build.SUPPORTED_ABIS.isNotEmpty()) {
            sendPart(gzipOs, boundary, "Android_CPU_ABI", Build.SUPPORTED_ABIS[0], nameSet)
            if (Build.SUPPORTED_ABIS.size >= 2) {
                sendPart(gzipOs, boundary, "Android_CPU_ABI2", Build.SUPPORTED_ABIS[1], nameSet)
            }
        }

        gzipOs.write(("\r\n--$boundary--\r\n").toByteArray())
        gzipOs.flush()
        gzipOs.close()
    }

    private fun sendProcessName(os: OutputStream, boundary: String, nameSet: MutableSet<String>) {
        val pid = android.os.Process.myPid()
        val manager = applicationContext.getSystemService(Context.ACTIVITY_SERVICE) as ActivityManager
        manager.runningAppProcesses.filter { it.pid == pid }.forEach {
            sendPart(os, boundary, "Android_ProcessName", it.processName, nameSet)
            return
        }
    }

    private fun sendPackageInstallTime(os: OutputStream, boundary: String, nameSet: MutableSet<String>) {
        val packageManager = applicationContext.packageManager
        try {
            val packageInfo = packageManager.getPackageInfo(applicationContext.packageName, 0)
            sendPart(
                os,
                boundary,
                "InstallTime",
                TimeUnit.MILLISECONDS.toSeconds(
                    packageInfo.lastUpdateTime,
                ).toString(),
                nameSet,
            )
        } catch (e: PackageManager.NameNotFoundException) {
            logger.error("Error getting package info", e)
        }
    }

    private fun generateBoundary(): String {
        val r0 = Random.nextInt(0, Int.MAX_VALUE)
        val r1 = Random.nextInt(0, Int.MAX_VALUE)
        return String.format("---------------------------%08X%08X", r0, r1)
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal fun sendPart(
        os: OutputStream,
        boundary: String,
        name: String,
        data: String?,
        nameSet: MutableSet<String>,
    ) {
        if (data == null) {
            return
        }

        if (nameSet.contains(name)) {
            return
        } else {
            nameSet.add(name)
        }

        try {
            os.write(
                (
                    "--$boundary\r\nContent-Disposition: form-data; " +
                        "name=$name\r\n\r\n$data\r\n"
                    ).toByteArray(),
            )
        } catch (e: IOException) {
            logger.error("Exception when sending $name", e)
        }
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal fun sendFile(
        os: OutputStream,
        boundary: String,
        name: String,
        file: File,
        nameSet: MutableSet<String>,
    ) {
        if (nameSet.contains(name)) {
            return
        } else {
            nameSet.add(name)
        }

        try {
            os.write(
                (
                    "--${boundary}\r\n" +
                        "Content-Disposition: form-data; name=\"$name\"; " +
                        "filename=\"${file.getName()}\"\r\n" +
                        "Content-Type: application/octet-stream\r\n\r\n"
                    ).toByteArray(),
            )
        } catch (e: IOException) {
            logger.error("failed to write boundary", e)
            return
        }

        try {
            val fileInputStream = FileInputStream(file).channel
            fileInputStream.transferTo(0, fileInputStream.size(), Channels.newChannel(os))
            fileInputStream.close()
        } catch (e: IOException) {
            logger.error("failed to send file", e)
        }

        try {
            // Add EOL to separate from the next part
            os.write("\r\n".toByteArray())
        } catch (e: IOException) {
            logger.error("failed to write EOL", e)
        }
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal fun unescape(string: String): String {
        return string.replace("\\\\\\\\", "\\").replace("\\\\n", "\n").replace("\\\\t", "\t")
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal fun jsonUnescape(string: String): String {
        return string.replace("""\\\\""", "\\").replace("""\n""", "\n").replace("""\t""", "\t")
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    @Suppress("NestedBlockDepth")
    internal fun readExtrasFromLegacyFile(file: File): HashMap<String, String> {
        var fileReader: FileReader? = null
        var bufReader: BufferedReader? = null
        var line: String?
        val map = HashMap<String, String>()

        try {
            fileReader = FileReader(file)
            bufReader = BufferedReader(fileReader)
            line = bufReader.readLine()
            while (line != null) {
                val equalsPos = line.indexOf('=')
                if ((equalsPos) != -1) {
                    val key = line.substring(0, equalsPos)
                    val value = unescape(line.substring(equalsPos + 1))
                    if (!ignoreKeys.contains(key)) {
                        map[key] = value
                    }
                }
                line = bufReader.readLine()
            }
        } catch (e: IOException) {
            logger.error("failed to convert extras to map", e)
        } finally {
            try {
                fileReader?.close()
                bufReader?.close()
            } catch (e: IOException) {
                // do nothing
            }
        }

        return map
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    @Suppress("NestedBlockDepth")
    internal fun readExtrasFromFile(file: File): HashMap<String, String> {
        var resultMap = HashMap<String, String>()
        var notJson = false

        try {
            FileReader(file).use { fileReader ->
                val input = fileReader.readLines().firstOrNull()
                    ?: throw JSONException("failed to read json file")

                val jsonObject = JSONObject(input)
                for (key in jsonObject.keys()) {
                    if (!ignoreKeys.contains(key)) {
                        resultMap[key] = jsonUnescape(jsonObject.getString(key))
                    }
                }
            }
        } catch (e: FileNotFoundException) {
            logger.error("failed to find extra file", e)
        } catch (e: IOException) {
            logger.error("failed read the extra file", e)
        } catch (e: JSONException) {
            logger.info("extras file JSON syntax error, trying legacy format")
            notJson = true
        }

        if (notJson) {
            resultMap = readExtrasFromLegacyFile(file)
        }

        return resultMap
    }

    private fun getExceptionStackTrace(throwable: Throwable, isCaughtException: Boolean): String {
        return when (isCaughtException) {
            true -> "$LIB_CRASH_INFO_PREFIX ${throwable.getStacktraceAsString()}"
            false -> throwable.getStacktraceAsString()
        }
    }
}
