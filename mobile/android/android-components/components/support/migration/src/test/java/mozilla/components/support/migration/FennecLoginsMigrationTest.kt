/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.migration

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.concept.base.crash.CrashReporting
import mozilla.components.service.sync.logins.ServerPassword
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.verifyNoInteractions
import java.io.File
import java.lang.IllegalArgumentException

@RunWith(AndroidJUnit4::class)
class FennecLoginsMigrationTest {
    @Test
    fun `verify master password`() {
        assertTrue(
            FennecLoginsMigration.isMasterPasswordValid(
                masterPassword = "",
                key4DbPath = File(getTestPath("logins"), "basic/key4.db").absolutePath
            )
        )

        assertFalse(
            FennecLoginsMigration.isMasterPasswordValid(
                masterPassword = "incorrect password",
                key4DbPath = File(getTestPath("logins"), "basic/key4.db").absolutePath
            )
        )

        assertFalse(
            FennecLoginsMigration.isMasterPasswordValid(
                masterPassword = "",
                key4DbPath = File(getTestPath("logins"), "with-mp/key4.db").absolutePath
            )
        )

        assertTrue(
            FennecLoginsMigration.isMasterPasswordValid(
                masterPassword = "my secret pass",
                key4DbPath = File(getTestPath("logins"), "with-mp/key4.db").absolutePath
            )
        )
    }

    @Test
    fun `decrypted records without a set master password`() {
        val crashReporter: CrashReporting = mock()
        with(
            FennecLoginsMigration.getLogins(
                crashReporter,
                masterPassword = FennecLoginsMigration.DEFAULT_MASTER_PASSWORD,
                signonsDbPath = File(getTestPath("logins"), "basic/signons.sqlite").absolutePath,
                key4DbPath = File(getTestPath("logins"), "basic/key4.db").absolutePath
            )
        ) {
            assertEquals(2, this.totalRecordsDetected)
            assertEquals(
                listOf(
                    ServerPassword(
                        id = "{cf7cabe4-ec82-4800-b077-c6d97ffcd63a}",
                        hostname = "https://html.com",
                        username = "",
                        password = "testp",
                        httpRealm = null,
                        formSubmitUrl = "https://html.com",
                        timesUsed = 1,
                        timeCreated = 1574735237274,
                        timeLastUsed = 1574735237274,
                        timePasswordChanged = 1574735237274,
                        usernameField = "",
                        passwordField = "password"
                    ),
                    ServerPassword(
                        id = "{390e62d7-77ef-4907-bc03-4c8010543a36}",
                        hostname = "https://getbootstrap.com",
                        username = "test@example.com",
                        password = "super duper pass 1",
                        httpRealm = null,
                        formSubmitUrl = "https://getbootstrap.com",
                        timesUsed = 1,
                        timeCreated = 1574735368749,
                        timeLastUsed = 1574735368749,
                        timePasswordChanged = 1574735368749,
                        usernameField = "",
                        passwordField = ""
                    )
                ),
                this.records
            )
        }
        verifyNoInteractions(crashReporter)
    }

    @Test
    fun `decrypted record with long encoded strings without a set master password`() {
        val crashReporter: CrashReporting = mock()
        with(
            FennecLoginsMigration.getLogins(
                crashReporter,
                masterPassword = FennecLoginsMigration.DEFAULT_MASTER_PASSWORD,
                signonsDbPath = File(getTestPath("logins"), "basic/longPass-signons.sqlite").absolutePath,
                key4DbPath = File(getTestPath("logins"), "basic/longPass-key4.db").absolutePath
            )
        ) {
            assertEquals(2, this.totalRecordsDetected)
            assertEquals(
                listOf(
                    ServerPassword(
                        id = "{0a5e8c6c-11bd-49e4-a09f-cf33c3b65498}",
                        hostname = "https://fiddle.jshell.net",
                        // This triggers a long-form length encoding with one extra length byte.
                        username = "not-a-real-email-of-course-but-rather-a-long-string-for-testing-however-do-see-the-domain-itself-it-is-real-and-interesting@thelongestdomainnameintheworldandthensomeandthensomemoreandmore.com",
                        // This triggers a long-form length encoding with two extra length bytes.
                        password = "IzLKPAKu6W3JX3wqi6WyIxJWeqckDkNiqrKLZAZ3MiWSTS3d5ELHLInrF97tsb86ygawBldjc8JMiWmkdRcC5ZlxMDs4LPqeu8Fvh6SYCZhcxiSd7fabYdW0JUlX7CSPNkgVJR08rtgQluyfqWJiEMoei1VrvLEiEuHIZpD1S02In8PBFF41gDLFGRX6XiA2OiyuKMXdudC8CjGZ5OltrIEurjIbIlTvCyXvz6nT0cWFmMVKsOroMSXwDlFFIxVebakjSn7cIEFpyFHhPWtKzazO1eQj3lBHA1fiduUBLFOrgRngDtvxeed2kTjKwj7kz8wCEjNYKp9k0g7Kw8ZLw4N1CYyLglULZ9WublNdKeN4a4LRSGpCEFB9Ey7ZvUN7pGY7CSWDzMCa1Fbgfhci2W0q5BaftMMerehoLKS7BweSzJMnmBiRgdEVjZ1i1hfeL7e23Tbo6B18SgVoZDZIQicBdPcWHogOyf5nrdzU9cGAzNjsCtjb18WmB7ow4hok5FThYx7rXtzczem5us5AvZGOE6dsqdciIO8mftawRy200KdAFZjfFQ4ToJMuk2tGJo9vB8MuoXvkTzgPJCs7R9rYkdFlMuJujKIhY5D43Zh0EQYFGGlN1vCc7vpPAqzC7f07fq0v2PR6ox7vmifsVEgGzfioOx57L7lEQWCiAM81wd2gwHg4SUyXDst1avyaDZljLdXdxjPPIQgfbwl8kCSMtKLKuvNySWCl64ZIzT0rULWv1rrxqlVmI2jSLbTnB6Tni7MH0rX5lmBO8W9EdsOJUV2ekMuGJOVVqb1bLGPGUHJvQk25edHbCbVCCryqq5SPoGwEBXFtNNVXC4ajePKQK78NH3LVmsqwQVq0ITgDqd98NCoHmciIdmZ6cnjjFDsLwooK9WHA8Deq0li9zSRKYn8ZrFMQ28R88Mxpo4aMlWjLmApBHDMmwy588VuuuwjuQruaja8VcVZEP05A7Y0d2g9LpCUC1KChlcR4cPWL6xXuOPJTlheTuen37VFv",
                        httpRealm = null,
                        formSubmitUrl = "https://fiddle.jshell.net",
                        timesUsed = 0,
                        timeCreated = 1575341384499,
                        timeLastUsed = 0,
                        timePasswordChanged = 1575411364366,
                        usernameField = "",
                        passwordField = ""
                    ),
                    ServerPassword(
                        id = "{30639772-dca9-420b-945d-48686df67494}",
                        hostname = "https://getbootstrap.com",
                        // This triggers a long-form length encoding with three extra length bytes.
                        username = "a".repeat(65537) + "'@test.com",
                        password = "hello",
                        httpRealm = null,
                        formSubmitUrl = "https://getbootstrap.com",
                        timesUsed = 0,
                        timeCreated = 1575415115144,
                        timeLastUsed = 0,
                        timePasswordChanged = 1575415115143,
                        usernameField = "",
                        passwordField = ""
                    )
                ),
                this.records
            )
        }
        verifyNoInteractions(crashReporter)
    }

    @Test
    fun `decrypted records with a master password`() {
        val crashReporter: CrashReporting = mock()
        with(
            FennecLoginsMigration.getLogins(
                crashReporter,
                masterPassword = "my secret pass",
                signonsDbPath = File(getTestPath("logins"), "with-mp/signons.sqlite").absolutePath,
                key4DbPath = File(getTestPath("logins"), "with-mp/key4.db").absolutePath
            )
        ) {
            assertEquals(2, this.totalRecordsDetected)
            assertEquals(
                listOf(
                    ServerPassword(
                        id = "{cf7cabe4-ec82-4800-b077-c6d97ffcd63a}",
                        hostname = "https://html.com",
                        username = "",
                        password = "testp",
                        httpRealm = null,
                        formSubmitUrl = "https://html.com",
                        timesUsed = 1,
                        timeCreated = 1574735237274,
                        timeLastUsed = 1574735237274,
                        timePasswordChanged = 1574735237274,
                        usernameField = "",
                        passwordField = "password"
                    ),
                    ServerPassword(
                        id = "{390e62d7-77ef-4907-bc03-4c8010543a36}",
                        hostname = "https://getbootstrap.com",
                        username = "test@example.com",
                        password = "super duper pass 1",
                        httpRealm = null,
                        formSubmitUrl = "https://getbootstrap.com",
                        timesUsed = 1,
                        timeCreated = 1574735368749,
                        timeLastUsed = 1574735368749,
                        timePasswordChanged = 1574735368749,
                        usernameField = "",
                        passwordField = ""
                    )
                ),
                this.records
            )
        }
        verifyNoInteractions(crashReporter)
    }

    @Test
    fun `exception collector`() {
        val exceptionCollector: MutableMap<String, Exception> = mutableMapOf()
        FennecLoginsMigration.collectExceptionsIntoASet(exceptionCollector) {}
        assertEquals(0, exceptionCollector.size)

        FennecLoginsMigration.collectExceptionsIntoASet(exceptionCollector) {
            throw IllegalArgumentException("boo")
        }
        assertEquals(1, exceptionCollector.size)

        FennecLoginsMigration.collectExceptionsIntoASet(exceptionCollector) {
            throw IllegalArgumentException("boo")
        }
        // Exception occurs at a different line number in the file.
        assertEquals(2, exceptionCollector.size)

        FennecLoginsMigration.collectExceptionsIntoASet(exceptionCollector) {
            throw IllegalArgumentException("blah")
        }
        // Exception occurs at a different line number in the file.
        assertEquals(3, exceptionCollector.size)

        // New exception entirely.
        FennecLoginsMigration.collectExceptionsIntoASet(exceptionCollector, ::throwingFunction)
        assertEquals(4, exceptionCollector.size)

        // Repeat the same exact exception as above.
        FennecLoginsMigration.collectExceptionsIntoASet(exceptionCollector, ::throwingFunction)
        assertEquals(4, exceptionCollector.size)
    }

    private fun throwingFunction() {
        throw IllegalArgumentException("where are ma argz!")
    }

    @Test
    fun testGetEncodingLength() {
        // Short form
        assertEquals(0, byteArrayOf(0, 0x00.toByte()).getEncodingLength(0))
        assertEquals(3, byteArrayOf(0, 0x03.toByte()).getEncodingLength(0))
        assertEquals(127, byteArrayOf(0, 0x7F.toByte()).getEncodingLength(0))
        // Long form 1 byte
        assertEquals(0, byteArrayOf(0, 0x81.toByte(), 0x00.toByte()).getEncodingLength(0))
        assertEquals(3, byteArrayOf(0, 0x81.toByte(), 0x03.toByte()).getEncodingLength(0))
        assertEquals(127, byteArrayOf(0, 0x81.toByte(), 0x7F.toByte()).getEncodingLength(0))
        assertEquals(128, byteArrayOf(0, 0x81.toByte(), 0x80.toByte()).getEncodingLength(0))
        assertEquals(255, byteArrayOf(0, 0x81.toByte(), 0xFF.toByte()).getEncodingLength(0))
        // Long form 2 bytes
        assertEquals(0, byteArrayOf(0, 0x82.toByte(), 0x00.toByte(), 0x00.toByte()).getEncodingLength(0))
        assertEquals(3, byteArrayOf(0, 0x82.toByte(), 0x00.toByte(), 0x03.toByte()).getEncodingLength(0))
        assertEquals(4660, byteArrayOf(0, 0x82.toByte(), 0x12.toByte(), 0x34.toByte()).getEncodingLength(0))
        assertEquals(32896, byteArrayOf(0, 0x82.toByte(), 0x80.toByte(), 0x80.toByte()).getEncodingLength(0))
        assertEquals(255, byteArrayOf(0, 0x82.toByte(), 0x00.toByte(), 0xFF.toByte()).getEncodingLength(0))
        assertEquals(65280, byteArrayOf(0, 0x82.toByte(), 0xFF.toByte(), 0x00.toByte()).getEncodingLength(0))
        assertEquals(511, byteArrayOf(0, 0x82.toByte(), 0x01.toByte(), 0xFF.toByte()).getEncodingLength(0))
        assertEquals(32639, byteArrayOf(0, 0x82.toByte(), 0x7F.toByte(), 0x7F.toByte()).getEncodingLength(0))
        // Long form 3 bytes
        assertEquals(0, byteArrayOf(0, 0x83.toByte(), 0x00.toByte(), 0x00.toByte(), 0x00.toByte()).getEncodingLength(0))
        assertEquals(3, byteArrayOf(0, 0x83.toByte(), 0x00.toByte(), 0x00.toByte(), 0x03.toByte()).getEncodingLength(0))
        assertEquals(8421504, byteArrayOf(0, 0x83.toByte(), 0x80.toByte(), 0x80.toByte(), 0x80.toByte()).getEncodingLength(0))
        assertEquals(32896, byteArrayOf(0, 0x83.toByte(), 0x00.toByte(), 0x80.toByte(), 0x80.toByte()).getEncodingLength(0))
        assertEquals(128, byteArrayOf(0, 0x83.toByte(), 0x00.toByte(), 0x00.toByte(), 0x80.toByte()).getEncodingLength(0))
        assertEquals(255, byteArrayOf(0, 0x83.toByte(), 0x00.toByte(), 0x00.toByte(), 0xFF.toByte()).getEncodingLength(0))
        assertEquals(65535, byteArrayOf(0, 0x83.toByte(), 0x00.toByte(), 0xFF.toByte(), 0xFF.toByte()).getEncodingLength(0))
        assertEquals(16777215, byteArrayOf(0, 0x83.toByte(), 0xFF.toByte(), 0xFF.toByte(), 0xFF.toByte()).getEncodingLength(0))
    }
}
