/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.glean

import android.content.Context
import android.content.pm.PackageInfo
import android.content.pm.PackageManager
import androidx.test.core.app.ApplicationProvider
import mozilla.components.service.glean.config.Configuration
import mozilla.components.service.glean.ping.PingMaker
import mozilla.components.service.glean.storages.StorageEngineManager
import org.json.JSONObject
import org.mockito.ArgumentMatchers
import org.mockito.Mockito

/**
 * Checks ping content against the glean ping schema.
 *
 * This uses the Python utility, glean_parser, to perform the actual checking.
 * This is installed in its own Miniconda environment as part of the build
 * configuration in sdk_generator.gradle.
 *
 * @param content The JSON content of the ping
 * @throws AssertionError If the JSON content is not valid
 */
internal fun checkPingSchema(content: JSONObject) {
    val os = System.getProperty("os.name")?.toLowerCase()
    val pythonExecutable =
        if (os?.indexOf("win")?.compareTo(0) == 0)
            "${BuildConfig.GLEAN_MINICONDA_DIR}/python"
        else
            "${BuildConfig.GLEAN_MINICONDA_DIR}/bin/python"

    val proc = ProcessBuilder(
        listOf(
            pythonExecutable,
            "-m",
            "glean_parser",
            "check",
            "-s",
            "${BuildConfig.GLEAN_PING_SCHEMA_URL}"
        )
    ).redirectOutput(ProcessBuilder.Redirect.INHERIT)
        .redirectError(ProcessBuilder.Redirect.INHERIT)
    val process = proc.start()

    val jsonString = content.toString()
    with(process.outputStream.bufferedWriter()) {
        write(jsonString)
        newLine()
        flush()
        close()
    }

    val exitCode = process.waitFor()
    assert(exitCode == 0)
}

/**
 * Checks ping content against the glean ping schema.
 *
 * This uses the Python utility, glean_parser, to perform the actual checking.
 * This is installed in its own Miniconda environment as part of the build
 * configuration in sdk_generator.gradle.
 *
 * @param content The JSON content of the ping
 * @return the content string, parsed into a JSONObject
 * @throws AssertionError If the JSON content is not valid
 */
internal fun checkPingSchema(content: String): JSONObject {
    val jsonContent = JSONObject(content)
    checkPingSchema(jsonContent)
    return jsonContent
}

/**
 * Collects a specified ping type and checks it against the glean ping schema.
 *
 * @param storeName The name of the ping to check
 * @return the ping contents, in a JSONObject
 * @throws AssertionError If the JSON content is not valid
 */
internal fun collectAndCheckPingSchema(storeName: String): JSONObject {
    val appContext = ApplicationProvider.getApplicationContext<Context>()
    val jsonString = PingMaker(
        StorageEngineManager(applicationContext = appContext),
        appContext
    ).collect(storeName)!!
    return checkPingSchema(jsonString)
}

/**
 * Resets the glean state and trigger init again.
 *
 * @param context the application context to init glean with
 * @param config the [Configuration] to init glean with
 */
internal fun resetGlean(
    context: Context = ApplicationProvider.getApplicationContext(),
    config: Configuration = Configuration()
) {
    Glean.initialized = false
    Glean.initialize(context, config)
}

/**
 * Get a context that contains [PackageInfo.versionName] mocked to
 * "glean.version.name".
 *
 * @return an application [Context] that can be used to init glean
 */
internal fun getContextWithMockedInfo(): Context {
    val context = Mockito.spy<Context>(ApplicationProvider.getApplicationContext<Context>())
    val packageInfo = Mockito.mock(PackageInfo::class.java)
    packageInfo.versionName = "glean.version.name"
    val packageManager = Mockito.mock(PackageManager::class.java)
    Mockito.`when`(packageManager.getPackageInfo(ArgumentMatchers.anyString(), ArgumentMatchers.anyInt())).thenReturn(packageInfo)
    Mockito.`when`(context.packageManager).thenReturn(packageManager)
    return context
}
