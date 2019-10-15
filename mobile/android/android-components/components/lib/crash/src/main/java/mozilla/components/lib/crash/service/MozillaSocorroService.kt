/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.crash.service

import android.app.ActivityManager
import android.content.Context
import android.content.pm.PackageManager
import android.os.Build
import androidx.annotation.VisibleForTesting
import mozilla.components.lib.crash.Crash
import mozilla.components.support.base.log.logger.Logger
import org.json.JSONException
import org.json.JSONObject
import org.mozilla.geckoview.BuildConfig
import java.io.BufferedReader
import java.io.File
import java.io.FileInputStream
import java.io.FileNotFoundException
import java.io.FileReader
import java.io.IOException
import java.io.InputStreamReader
import java.io.OutputStream
import java.io.PrintWriter
import java.io.StringWriter
import java.net.HttpURLConnection
import java.net.URL
import java.nio.channels.Channels
import java.util.concurrent.TimeUnit
import java.util.zip.GZIPOutputStream
import kotlin.collections.HashMap
import kotlin.random.Random

/* This ID is used for all Mozilla products.  Setting as default if no ID is passed in */
private const val MOZILLA_PRODUCT_ID = "{eeb82917-e434-4870-8148-5c03d4caa81b}"

/**
 * A [CrashReporterService] implementation uploading crash reports to crash-stats.mozilla.com.
 *
 * @param applicationContext The application [Context].
 * @param appName A human-readable app name. This name is used on crash-stats.mozilla.com to filter crashes by app.
 *                The name needs to be whitelisted for the server to accept the crash.
 *                [File a bug](https://bugzilla.mozilla.org/enter_bug.cgi?product=Socorro) if you would like to get your
 *                app added to the whitelist.
 */
@Suppress("TooManyFunctions", "LargeClass")
class MozillaSocorroService(
    private val applicationContext: Context,
    private val appName: String,
    private val appId: String = MOZILLA_PRODUCT_ID,
    private val version: String = BuildConfig.MOZILLA_VERSION,
    private val buildId: String = BuildConfig.MOZ_APP_BUILDID,
    private val vendor: String = BuildConfig.MOZ_APP_VENDOR,
    private val serverUrl: String = "https://crash-reports.mozilla.com/submit?id=$appId&version=$version&$buildId"
) : CrashReporterService {
    private val logger = Logger("mozac/MozillaSocorroCrashHelperService")
    private val startTime = System.currentTimeMillis()

    override fun report(crash: Crash.UncaughtExceptionCrash) {
        sendReport(crash.throwable, null, null, false)
    }

    override fun report(crash: Crash.NativeCodeCrash) {
        sendReport(null, crash.minidumpPath, crash.extrasPath, false)
    }

    override fun report(throwable: Throwable) {
        sendReport(throwable, null, null, true)
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal fun sendReport(
        throwable: Throwable?,
        miniDumpFilePath: String?,
        extrasFilePath: String?,
        isCaughtException: Boolean
    ) {
        val url = URL(serverUrl)
        val boundary = generateBoundary()
        var conn: HttpURLConnection? = null

        try {
            conn = url.openConnection() as HttpURLConnection
            conn.requestMethod = "POST"
            conn.doOutput = true
            conn.setRequestProperty("Content-Type", "multipart/form-data; boundary=$boundary")
            conn.setRequestProperty("Content-Encoding", "gzip")

            sendCrashData(conn.outputStream, boundary, throwable, miniDumpFilePath, extrasFilePath,
                    isCaughtException)

            BufferedReader(InputStreamReader(conn.inputStream)).use {
                val response = StringBuffer()
                var inputLine = it.readLine()
                while (inputLine != null) {
                    response.append(inputLine)
                    inputLine = it.readLine()
                }

                Logger.info("Crash reported to Socorro: $response")
            }
        } catch (e: IOException) {
            Logger.error("failed to send report to Socorro", e)
        } finally {
            conn?.disconnect()
        }
    }

    @Suppress("LongParameterList")
    private fun sendCrashData(
        os: OutputStream,
        boundary: String,
        throwable: Throwable?,
        miniDumpFilePath: String?,
        extrasFilePath: String?,
        isCaughtException: Boolean
    ) {
        val nameSet = mutableSetOf<String>()
        val gzipOs = GZIPOutputStream(os)
        sendPart(gzipOs, boundary, "ProductName", appName, nameSet)
        sendPart(gzipOs, boundary, "ProductID", appId, nameSet)
        sendPart(gzipOs, boundary, "Version", version, nameSet)
        sendPart(gzipOs, boundary, "BuildID", buildId, nameSet)
        sendPart(gzipOs, boundary, "Vendor", vendor, nameSet)

        extrasFilePath?.let {
            val extrasFile = File(it)
            val extrasMap = readExtrasFromFile(extrasFile)
            for (key in extrasMap.keys) {
                sendPart(gzipOs, boundary, key, extrasMap[key], nameSet)
            }
            extrasFile.delete()
        }

        throwable?.let {
            sendPart(gzipOs, boundary, "JavaStackTrace", getExceptionStackTrace(it,
                    isCaughtException), nameSet)
        }

        miniDumpFilePath?.let {
            val minidumpFile = File(it)
            sendFile(gzipOs, boundary, "upload_file_minidump", minidumpFile, nameSet)
            minidumpFile.delete()
        }

        sendPackageInstallTime(gzipOs, boundary, nameSet)
        sendProcessName(gzipOs, boundary, nameSet)
        sendPart(gzipOs, boundary, "ReleaseChannel", BuildConfig.MOZ_UPDATE_CHANNEL, nameSet)
        sendPart(gzipOs, boundary, "StartupTime",
                TimeUnit.MILLISECONDS.toSeconds(startTime).toString(), nameSet)
        sendPart(gzipOs, boundary, "CrashTime",
                TimeUnit.MILLISECONDS.toSeconds(System.currentTimeMillis()).toString(), nameSet)
        sendPart(gzipOs, boundary, "Android_PackageName", applicationContext.packageName, nameSet)
        sendPart(gzipOs, boundary, "Android_Manufacturer", Build.MANUFACTURER, nameSet)
        sendPart(gzipOs, boundary, "Android_Model", Build.MODEL, nameSet)
        sendPart(gzipOs, boundary, "Android_Board", Build.BOARD, nameSet)
        sendPart(gzipOs, boundary, "Android_Brand", Build.BRAND, nameSet)
        sendPart(gzipOs, boundary, "Android_Device", Build.DEVICE, nameSet)
        sendPart(gzipOs, boundary, "Android_Display", Build.DISPLAY, nameSet)
        sendPart(gzipOs, boundary, "Android_Fingerprint", Build.FINGERPRINT, nameSet)
        sendPart(gzipOs, boundary, "Android_Hardware", Build.HARDWARE, nameSet)
        sendPart(gzipOs, boundary, "Android_Version",
                "${Build.VERSION.SDK_INT} (${Build.VERSION.CODENAME})", nameSet)
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
            sendPart(os, boundary, "InstallTime", TimeUnit.MILLISECONDS.toSeconds(
                    packageInfo.lastUpdateTime).toString(), nameSet)
        } catch (e: PackageManager.NameNotFoundException) {
            Logger.error("Error getting package info", e)
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
        nameSet: MutableSet<String>
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
            os.write(("--$boundary\r\nContent-Disposition: form-data; " +
                    "name=$name\r\n\r\n$data\r\n").toByteArray())
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
        nameSet: MutableSet<String>
    ) {
        if (nameSet.contains(name)) {
            return
        } else {
            nameSet.add(name)
        }

        try {
            os.write(("--${boundary}\r\n" +
                    "Content-Disposition: form-data; name=\"$name\"; " +
                    "filename=\"${file.getName()}\"\r\n" +
                    "Content-Type: application/octet-stream\r\n\r\n").toByteArray())
            val fileInputStream = FileInputStream(file).channel
            fileInputStream.transferTo(0, fileInputStream.size(), Channels.newChannel(os))
            fileInputStream.close()
        } catch (e: IOException) {
            Logger.error("failed to send file", e)
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
                    map[key] = value
                }
                line = bufReader.readLine()
            }
        } catch (e: IOException) {
            Logger.error("failed to convert extras to map", e)
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
    internal fun readExtrasFromFile(file: File): HashMap<String, String> {
        var resultMap = HashMap<String, String>()
        var notJson = false

        try {
            FileReader(file).use { fileReader ->
                val input = fileReader.readLines().firstOrNull()
                        ?: throw JSONException("failed to read json file")

                val jsonObject = JSONObject(input)
                for (key in jsonObject.keys()) {
                    resultMap[key] = jsonUnescape(jsonObject.getString(key))
                }
            }
        } catch (e: FileNotFoundException) {
            Logger.error("failed to find extra file", e)
        } catch (e: IOException) {
            Logger.error("failed read the extra file", e)
        } catch (e: JSONException) {
            Logger.info("extras file JSON syntax error, trying legacy format")
            notJson = true
        }

        if (notJson) {
            resultMap = readExtrasFromLegacyFile(file)
        }

        return resultMap
    }

    private fun getExceptionStackTrace(throwable: Throwable, isCaughtException: Boolean): String {
        val stringWriter = StringWriter()
        val printWriter = PrintWriter(stringWriter)
        throwable.printStackTrace(printWriter)
        printWriter.flush()

        return when (isCaughtException) {
            true -> "$INFO_PREFIX $stringWriter"
            false -> stringWriter.toString()
        }
    }
}
