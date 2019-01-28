/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.glean

import android.content.Context
import android.content.pm.PackageInfo
import android.content.pm.PackageManager
import androidx.test.core.app.ApplicationProvider
import mozilla.components.service.glean.config.Configuration
import org.json.JSONObject
import org.mockito.ArgumentMatchers
import org.mockito.Mockito

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
