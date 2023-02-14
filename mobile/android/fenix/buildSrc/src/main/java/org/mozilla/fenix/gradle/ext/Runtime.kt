/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.gradle.ext

import java.util.concurrent.TimeUnit

/**
 * Executes the given command with [Runtime.exec], throwing if the command returns a non-zero exit
 * code or times out. If successful, returns the command's stdout.
 *
 * @return stdout of the command
 * @throws [IllegalStateException] if the command returns a non-zero exit code or times out.
 */
fun Runtime.execReadStandardOutOrThrow(cmd: Array<String>, timeoutSeconds: Long = 30): String {
    val process = Runtime.getRuntime().exec(cmd)

    check(process.waitFor(timeoutSeconds, TimeUnit.SECONDS)) { "command unexpectedly timed out: `$cmd`" }
    check(process.exitValue() == 0) {
        val stderr = process.errorStream.bufferedReader().readText().trim()
        """command exited with non-zero exit value: ${process.exitValue()}.
           |cmd: ${cmd.joinToString(separator = " ")}
           |stderr:
           |${stderr}""".trimMargin()
    }

    return process.inputStream.bufferedReader().readText().trim()
}
