/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.helpers

/**
 * A collection of test helper functions for manipulating stack traces.
 */
object StackTraces {

    /**
     * Gets a stack trace from logcat output. To use this, you should remove the name of the
     * Exception or "Caused by" lines causing the problem and only use the stack trace lines below
     * it. See src/test/resources/EdmStorageProviderBaseLogcat.txt for an example.
     */
    fun getStackTraceFromLogcat(logcatResourcePath: String): Array<StackTraceElement> {
        val logcat = javaClass.classLoader!!.getResource(logcatResourcePath).readText()
        val lines = logcat.split('\n').filter(String::isNotBlank)
        return lines.map(::logcatLineToStackTraceElement).toTypedArray()
    }

    private fun logcatLineToStackTraceElement(line: String): StackTraceElement {
        // Expected format:
        // 02-08 10:56:02.185 21990 21990 E AndroidRuntime: 	at android.os.StrictMode$AndroidBlockGuardPolicy.onReadFromDisk(StrictMode.java:1556)

        // Expected: android.os.StrictMode$AndroidBlockGuardPolicy.onReadFromDisk
        val methodInfo = line.substringBefore('(').substringAfterLast(' ')
        val methodName = methodInfo.substringAfterLast('.')
        val declaringClass = methodInfo.substringBeforeLast('.')

        // Expected: StrictMode.java:1556
        val fileInfo = line.substringAfter('(').substringBefore(')')
        val fileName = fileInfo.substringBefore(':')
        val lineNumber = fileInfo.substringAfter(':').toInt()

        return StackTraceElement(declaringClass, methodName, fileName, lineNumber)
    }
}
