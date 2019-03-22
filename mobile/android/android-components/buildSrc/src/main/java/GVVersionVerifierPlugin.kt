/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import GeckoVersions.beta_version
import GeckoVersions.nightly_version
import GeckoVersions.release_version
import groovy.util.Node
import groovy.util.NodeList
import groovy.util.XmlParser
import org.gradle.api.Plugin
import org.gradle.api.Project
import java.io.File
import java.lang.Exception

open class GVVersionVerifierPlugin : Plugin<Project> {

    companion object {
        private const val GV_VERSION_PATH_FILE = "buildSrc/src/main/java/Gecko.kt"
        private const val MAVEN_MOZILLA_NIGHTLY_GV_URL =
            "https://maven.mozilla.org/maven2/org/mozilla/geckoview/geckoview-nightly-armeabi-v7a/maven-metadata.xml"
        private const val MAVEN_MOZILLA_BETA_GV_URL =
            "https://maven.mozilla.org/maven2/org/mozilla/geckoview/geckoview-beta-armeabi-v7a/maven-metadata.xml"
        private const val MAVEN_MOZILLA_STABLE_GV_URL =
            "https://maven.mozilla.org/maven2/org/mozilla/geckoview/geckoview-armeabi-v7a/maven-metadata.xml"
    }

    override fun apply(project: Project) {

        project.task("updateGVNightlyVersion") {

            doLast {
                val configuration = GVConfiguration(
                    actualVersionMajor = nightly_version.extractMajor(),
                    variableName = "nightly_version",
                    mavenURL = project.getNightlyMavenURL()
                )
                updateGVVersion(project, configuration)
            }
        }

        project.task("updateGVBetaVersion") {

            doLast {
                val configuration = GVConfiguration(
                    actualVersionMajor = beta_version.extractMajor(),
                    variableName = "beta_version",
                    mavenURL = project.getBetaMavenURL()
                )
                updateGVVersion(project, configuration)
            }
        }

        project.task("updateGVStableVersion") {

            doLast {
                val configuration = GVConfiguration(
                    actualVersionMajor = release_version.extractMajor(),
                    variableName = "release_version",
                    mavenURL = project.getStableMavenURL()
                )
                updateGVVersion(project, configuration)
            }
        }
    }

    @Suppress("TooGenericExceptionThrown")
    private fun updateGVVersion(project: Project, config: GVConfiguration) {

        val newVersion = getLastGeckoViewVersion(config)

        if (newVersion.isNotEmpty()) {
            val filePath = project.property("gvVersionFile", GV_VERSION_PATH_FILE)
            updateConfigFileWithNewGVVersion(filePath, newVersion, config.variableName)
        } else {
            throw Exception("Unable to find a new version of GeckoViewNightly")
        }
    }

    @Suppress("UNCHECKED_CAST")
    private fun getLastGeckoViewVersion(config: GVConfiguration): String {
        val versioning = (XmlParser().parse(config.mavenURL)["versioning"] as List<Node>)
        val latest = versioning[0]["latest"] as List<Node>
        val value = ((latest.first().value() as NodeList)[0]).toString()

        val lastVersion = if (value.isNotEmpty()) value else ""

        val lastVersionMajor = lastVersion.extractMajor()

        return if (config.actualVersionMajor == lastVersionMajor) lastVersion else ""
    }

    private fun updateConfigFileWithNewGVVersion(path: String, newVersion: String, variableName: String) {
        val file = File(path)
        var fileContent = file.readText()
        fileContent = fileContent.replace(
            Regex("$variableName.*=.*"),
            "$variableName = \"$newVersion\""
        )
        file.writeText(fileContent)
        println("${file.name} file updated")
    }

    private class GVConfiguration(val actualVersionMajor: String, val variableName: String, val mavenURL: String)

    private fun String.extractMajor() = this.split(".").first()

    private fun Project.getNightlyMavenURL(): String {
        return project.property("gvMavenNightlyVersionURL", MAVEN_MOZILLA_NIGHTLY_GV_URL)
    }

    private fun Project.getBetaMavenURL(): String {
        return project.property("gvMavenBetaVersionURL", MAVEN_MOZILLA_BETA_GV_URL)
    }

    private fun Project.getStableMavenURL(): String {
        return project.property("gvMavenStableVersionURL", MAVEN_MOZILLA_STABLE_GV_URL)
    }
}
