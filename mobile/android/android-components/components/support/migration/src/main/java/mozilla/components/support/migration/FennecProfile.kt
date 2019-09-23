/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.migration

import android.content.Context
import mozilla.components.support.base.log.logger.Logger
import java.io.File
import java.io.IOException
import java.util.regex.Pattern

private val logger = Logger("FennecProfile")
private val profilePattern = Pattern.compile("\\[(.+)]")

/**
 * A profile of "Fennec" (Firefox for Android).
 */
data class FennecProfile(
    val name: String,
    val path: String,
    val default: Boolean
) {
    companion object {
        /**
         * Returns the default [FennecProfile] - the default profile used by Fennec or `null` if no
         * profile could be found.
         *
         * If no explicit default profile could be found then this method will try to return a valid
         * profile that may be treated as the default profile:
         * - If there's only one profile then this profile will be returned.
         * - If there are multiple profiles with no default then the first profile will be returned.
         */
        fun findDefault(
            context: Context,
            mozillaDirectory: File = getMozillaDirectory(context),
            fileName: String = "profiles.ini"
        ): FennecProfile? {
            return findDefaultProfile(mozillaDirectory, fileName)
        }
    }
}

private fun getMozillaDirectory(context: Context): File {
    return File(context.filesDir, "mozilla")
}

@Suppress("ReturnCount")
private fun findDefaultProfile(
    mozillaDirectory: File,
    fileName: String
): FennecProfile? {
    val profiles = try {
        findProfiles(mozillaDirectory, fileName)
    } catch (e: IOException) {
        logger.debug("IOException when reading profile")
        return null
    }

    if (profiles.isEmpty()) {
        logger.debug("No profiles found")
        return null
    }

    profiles.firstOrNull {
        it.default
    }?.let {
        logger.debug("Found default profile: $it")
        return it
    }

    if (profiles.size == 1) {
        val profile = profiles[0]
        logger.debug("No default found but using only existing profile:: $profile")
        return profile
    }

    logger.debug("Multiple profiles without a default. Returning first.")
    return profiles[0]
}

@Suppress("ComplexMethod")
@Throws(IOException::class)
private fun findProfiles(
    mozillaDirectory: File,
    fileName: String = "profiles.ini"
): List<FennecProfile> {
    // Implementation note: Fennec saves information about its profiles in mozilla/profiles.ini.
    // A Fennec installation can have multiple profiles and the profile names always contain a
    // salt causing them to be different for every installation.
    // More about profiles.ini: http://kb.mozillazine.org/Profiles.ini_file

    val profilesFile = File(mozillaDirectory, fileName)

    logger.debug("profiles.ini: ${profilesFile.exists()}")

    if (!profilesFile.exists()) {
        return emptyList()
    }

    val profiles = mutableListOf<TemporaryProfile>()
    var temporaryProfile: TemporaryProfile? = null
    var isGeneralSection = false

    profilesFile.readLines().map { it.trim() }.forEach { line ->
        val sectionMatcher = profilePattern.matcher(line)
        val split = line.split("=")

        when {
            line.isBlank() || line.startsWith(";") -> Unit
            sectionMatcher.matches() -> {
                val name = checkNotNull(sectionMatcher.group(1))
                if (name.equals("general", ignoreCase = true)) {
                    isGeneralSection = true
                } else {
                    isGeneralSection = false
                    temporaryProfile = TemporaryProfile(name).also {
                        profiles.add(it)
                    }
                }
            }
            isGeneralSection -> {
                logger.debug("Ignoring line in [general] section: $line")
            }
            split.size != 2 -> {
                logger.debug("Unknown line: $line")
            }
            split[0].trim().equals("Name", ignoreCase = true) -> {
                temporaryProfile?.name = split[1].trim()
            }
            split[0].trim().equals("IsRelative", ignoreCase = true) -> {
                temporaryProfile?.isRelative = split[1].trim() == "1"
            }
            split[0].trim().equals("Path", ignoreCase = true) -> {
                temporaryProfile?.path = split[1].trim()
            }
            split[0].trim().equals("Default", ignoreCase = true) -> {
                temporaryProfile?.default = split[1].trim() == "1"
            }
        }
    }

    logger.debug("Found ${profiles.size} profiles")

    return profiles.mapNotNull { it.toFennecProfile(mozillaDirectory) }
}

private data class TemporaryProfile(
    var name: String,
    var isRelative: Boolean = true,
    var path: String? = null,
    var default: Boolean = false
)

private fun TemporaryProfile.toFennecProfile(mozillaDirectory: File): FennecProfile? {
    val path = path ?: return null

    val absolutePath = if (isRelative) {
        File(mozillaDirectory, path).absolutePath
    } else {
        path
    }

    return FennecProfile(name, absolutePath, default)
}
