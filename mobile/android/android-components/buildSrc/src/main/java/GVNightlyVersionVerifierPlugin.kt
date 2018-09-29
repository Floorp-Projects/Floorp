/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import GeckoVersions.nightly_version
import groovy.util.Node
import groovy.util.NodeList
import groovy.util.XmlParser
import org.gradle.api.Plugin
import org.gradle.api.Project
import org.gradle.kotlin.dsl.task
import java.io.File
import java.lang.Exception

open class GVNightlyVersionVerifierPlugin : Plugin<Project> {

    companion object {
        private const val GV_VERSION_PATH_FILE = "buildSrc/src/main/java/Gecko.kt"
        private const val MAVEN_MOZILLA_GV_URL = "https://maven.mozilla.org/maven2/org/mozilla/geckoview/geckoview-nightly-armeabi-v7a/maven-metadata.xml"
    }

    override fun apply(project: Project) {

        project.task("updateGVNightlyVersion") {

            doLast {
                updateGVNightlyVersion(project)
            }
        }
    }

    private fun updateGVNightlyVersion(project: Project) {

        val newVersion = getLastGeckoViewNightlyVersion(project)

        if (newVersion.isNotEmpty()) {
            val filePath = project.property("gvVersionFile", GV_VERSION_PATH_FILE)
            updateConfigFileWithNewGVNightlyVersion(filePath, newVersion)
        } else {
            throw Exception("Unable to find a new version of GeckoViewNightly")
        }
    }

    @Suppress("UNCHECKED_CAST")
    private fun getLastGeckoViewNightlyVersion(project: Project): String {
        val mavenURL = project.property("gvMavenVersionURL", MAVEN_MOZILLA_GV_URL)
        val versioning = (XmlParser().parse(mavenURL)["versioning"] as List<Node>)
        val latest = versioning[0]["latest"] as List<Node>
        val value = ((latest.first().value() as NodeList)[0]).toString()

        val lastVersion = if (value.isNotEmpty()) value else ""

        val actualVersionMajor = nightly_version.split(".").first()
        val lastVersionMajor = lastVersion.split(".").first()

        return if (actualVersionMajor == lastVersionMajor) lastVersion else ""
    }

    private fun updateConfigFileWithNewGVNightlyVersion(path: String, newVersion: String) {
        val file = File(path)
        var fileContent = file.readText()
        fileContent = fileContent.replace(Regex("nightly_version.*=.*"),
                "nightly_version = \"$newVersion\"")
        file.writeText(fileContent)
        println("${file.name} file updated")
    }

}