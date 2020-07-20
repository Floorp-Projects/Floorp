/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.gradle.tasks

import org.gradle.api.DefaultTask
import org.gradle.api.tasks.Input
import org.gradle.api.tasks.TaskAction
import java.io.File

/**
 * Helper to write to the "customCheckRunText.md" file for Taskcluster.
 * Taskcluster uses this file to populate the "Details" section in the GitHub Checks panel UI.
 */
open class GithubDetailsTask : DefaultTask() {

    /**
     * Text to display in the Github Checks panel under "Details". Any markdown works here.
     * The text is written to a markdown file which is used by Taskcluster.
     * Links are automatically rewritten to point to the correct Taskcluster URL.
     */
    @Input
    var text: String = ""

    private val detailsFile = File("/builds/worker/github/customCheckRunText.md")
    private val suffix = "\n\n_(404 if compilation failed)_"

    /**
     * Captures the link name and URL in a markdown link.
     * i.e. "### [Hello](/world.html)" -> "/world.html"
     */
    private val markdownLinkRegex = """\[(.*)]\((.*)\)""".toRegex()

    @TaskAction
    fun writeFile() {
        val taskId = System.getenv("TASK_ID")
        val url = "https://firefoxci.taskcluster-artifacts.net/$taskId/0/public"
        val replaced = text.replace(markdownLinkRegex) { match ->
            val (_, linkName, linkUrl) = match.groupValues
            "[$linkName](${url + linkUrl})"
        }

        project.mkdir("/builds/worker/github")
        detailsFile.writeText(replaced + suffix)
    }
}
