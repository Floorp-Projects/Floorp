/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.migration

import mozilla.components.concept.base.crash.CrashReporting
import mozilla.components.concept.storage.Login
import mozilla.components.support.base.log.logger.Logger

/**
 * Helper class that allows:
 * - checking for presence of MP on the Fennec's logins database
 * - reading MP-protected logins from Fennec's logins database
 */
class FennecLoginsMPImporter(
    profile: FennecProfile
) {
    private val logger = Logger("FennecLoginsMPImporter")
    private val key4DbPath: String = "${profile.path}/key4.db"
    private val signonsDbPath: String = "${profile.path}/signons.sqlite"
    private val browserDbPath: String = "${profile.path}/browser.db"

    /**
     * @return 'true' if MP is detected, 'false' otherwise (and in case of any errors).
     */
    @Suppress("TooGenericExceptionCaught")
    fun hasMasterPassword(): Boolean {
        return if (!isFennecInstallation(browserDbPath)) {
            logger.info("Skipping MP check, not a Fennec install.")
            false
        } else {
            try {
                // MP is set if default password doesn't work.
                !FennecLoginsMigration.isMasterPasswordValid(FennecLoginsMigration.DEFAULT_MASTER_PASSWORD, key4DbPath)
            } catch (e: Exception) {
                logger.error("Failed to check MP validity", e)
                false
            }
        }
    }

    /**
     * Checks if provided [password] is able to unlock MP-protected logins storage.
     * @param password Password to check.
     * @return 'true' if [password] is correct, 'false otherwise (and in case of any errors).
     */
    @Suppress("TooGenericExceptionCaught")
    fun checkPassword(password: String): Boolean {
        return try {
            FennecLoginsMigration.isMasterPasswordValid(password, key4DbPath)
        } catch (e: Exception) {
            logger.error("Failed to check passwor", e)
            false
        }
    }

    /**
     * @param password An MP to use for decrypting the Fennec logins database.
     * @param crashReporter [CrashReporting] instance for recording encountered exceptions.
     * @return A list of [Login] records representing logins stored in Fennec.
     */
    @Suppress("TooGenericExceptionCaught")
    fun getLoginRecords(password: String, crashReporter: CrashReporting): List<Login> {
        return try {
            FennecLoginsMigration.getLogins(crashReporter, password, signonsDbPath, key4DbPath).records
        } catch (e: Exception) {
            logger.error("Failed to read protected logins", e)
            listOf()
        }
    }
}
