/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import org.gradle.api.Plugin
import org.gradle.api.Project
import org.gradle.api.initialization.Settings
import java.io.File
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale
import java.util.concurrent.TimeUnit

class ConfigPlugin : Plugin<Settings> {
    override fun apply(settings: Settings) = Unit
}

object Config {

    @JvmStatic
    private fun generateDebugVersionName(): String {
        val today = Date()
        // Append the year (2 digits) and week in year (2 digits). This will make it easier to distinguish versions and
        // identify ancient versions when debugging issues. However this will still keep the same version number during
        // the week so that we do not end up with a lot of versions in tools like Sentry. As an extra this matches the
        // sections we use in the changelog (weeks).
        return SimpleDateFormat("1.0.yyww", Locale.US).format(today)
    }

    @JvmStatic
    fun releaseVersionName(project: Project): String? {
        // Note: release builds must have the `versionName` set. However, the gradle ecosystem makes this hard to
        // ergonomically validate (sometimes IDEs default to a release variant and mysteriously fail due to the
        // validation, sometimes devs just need a release build and specifying project properties is annoying in IDEs),
        // so instead we'll allow the `versionName` to silently default to an empty string.
        return if (project.hasProperty("versionName")) project.property("versionName") as String else null
    }

    @JvmStatic
    fun nightlyVersionName(): String {
        // Nightly versions will use the version from "version.txt".
        return readVersionFromFile()
    }

    @JvmStatic
    fun readVersionFromFile(): String {
        return File("../version.txt").useLines { it.firstOrNull() ?: "" }
    }

    @JvmStatic
    fun majorVersion(): String {
        return readVersionFromFile().split(".")[0]
    }

    /**
     * Returns the git hash of the currently checked out revision. If there are uncommitted changes,
     * a "+" will be appended to the hash, e.g. "c8ba05ad0+".
     */
    @JvmStatic
    fun getGitHash(): String {
        val revisionCmd = arrayOf("git", "rev-parse", "--short", "HEAD")
        val revision = execReadStandardOutOrThrow(revisionCmd)

        // Append "+" if there are uncommitted changes in the working directory.
        val statusCmd = arrayOf("git", "status", "--porcelain=v2")
        val status = execReadStandardOutOrThrow(statusCmd)
        val hasUnstagedChanges = status.isNotBlank()
        val statusSuffix = if (hasUnstagedChanges) "+" else ""

        return "$revision$statusSuffix"
    }

    /**
     * Executes the given command with [Runtime.exec], throwing if the command returns a non-zero exit
     * code or times out. If successful, returns the command's stdout.
     *
     * @return stdout of the command
     * @throws [IllegalStateException] if the command returns a non-zero exit code or times out.
     */
    private fun execReadStandardOutOrThrow(cmd: Array<String>, timeoutSeconds: Long = 30): String {
        val process = Runtime.getRuntime().exec(cmd)

        check(
            process.waitFor(
                timeoutSeconds,
                TimeUnit.SECONDS,
            ),
        ) { "command unexpectedly timed out: `$cmd`" }
        check(process.exitValue() == 0) {
            val stderr = process.errorStream.bufferedReader().readText().trim()

            """command exited with non-zero exit value: ${process.exitValue()}.
           |cmd: ${cmd.joinToString(separator = " ")}
           |stderr:
           |$stderr"""
                .trimMargin()
        }

        return process.inputStream.bufferedReader().readText().trim()
    }
}
