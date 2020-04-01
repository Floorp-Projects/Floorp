/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.gradle.tasks

import org.gradle.api.DefaultTask
import org.gradle.api.GradleException
import org.gradle.api.tasks.TaskAction
import org.gradle.api.tasks.TaskExecutionException
import java.io.File

private val INVALID_TEST_RUNNERS = setOf(
    // When updating this list, also update the violation message.
    "AndroidJUnit4",
    "RobolectricTestRunner"
)

/**
 * A lint check that ensures unit tests do not specify an invalid test runner. See the error message
 * for details.
 *
 * Related notes:
 * - The AndroidJUnit4 runner seems to just delegate to robolectric. Since robolectric non-trivially
 * increases test runtime, using AndroidJUnit4 in place of RobolectricTestRunner is very misleading
 * about how the test will change.
 */
open class LintUnitTestRunner : DefaultTask() {
    init {
        group = "Verification"
        description = "Ensures unit tests do not specify an invalid test runner"
        attachToAssembleUnitTestTasks()
    }

    private fun attachToAssembleUnitTestTasks() {
        project.gradle.projectsEvaluated { project.tasks.let { tasks ->
            val assembleUnitTestTasks = tasks.filter {
                it.name.startsWith("assemble") && it.name.endsWith("UnitTest")
            }

            assembleUnitTestTasks.forEach {
                it.dependsOn(this@LintUnitTestRunner)
            }
        } }
    }

    @TaskAction
    fun lint() {
        val unitTestDir = File(project.projectDir, "/src/test")
        check(unitTestDir.exists()) {
            "Error in task impl: expected test directory - ${unitTestDir.absolutePath} - to exist"
        }

        val unitTestDirFileWalk = unitTestDir.walk().onEnter { true /* enter all dirs */ }.asSequence()
        val kotlinFileWalk = unitTestDirFileWalk.filter { it.name.endsWith(".kt") && it.isFile }
        check(kotlinFileWalk.count() > 0) { "Error in task impl: expected to walk > 0 test files" }

        val violatingFiles = kotlinFileWalk.filter { file ->
            file.useLines { lines -> lines.any(::isLineInViolation) }
        }.sorted().toList()

        if (violatingFiles.isNotEmpty()) {
            throwViolation(violatingFiles)
        }
    }

    private fun isLineInViolation(line: String): Boolean {
        val trimmed = line.trimStart()
        return INVALID_TEST_RUNNERS.any { invalid ->
            trimmed.startsWith("@RunWith($invalid::class)")
        }
    }

    private fun throwViolation(files: List<File>) {
        val failureHeader = """Lint failure: saw unexpected unit test runners. The following code blocks:
            |
            |    @RunWith(AndroidJUnit4::class)
            |    @Config(application = TestApplication::class)
            |OR
            |    @RunWith(RobolectricTestRunner::class)
            |    @Config(application = TestApplication::class)
            |
            |should be replaced with:
            |
            |    @RunWith(FenixRobolectricTestRunner::class)
            |
            |To reduce redundancy of setting @Config. No @Config specification is necessary because
            |the FenixRobolectricTestRunner sets it automatically.
            |
            |Relatedly, adding robolectric to a test increases its runtime non-trivially so please
            |ensure Robolectric is necessary before adding it.
            |
            |The following files were found to be in violation:
        """.trimMargin()

        val filesInViolation = files.map {
            "    ${it.relativeTo(project.rootDir)}"
        }

        val errorMsg = (listOf(failureHeader) + filesInViolation).joinToString("\n")
        throw TaskExecutionException(this, GradleException(errorMsg))
    }
}
