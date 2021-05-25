/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:Suppress("Deprecation")

package org.mozilla.fenix.detektrules.perf

import io.gitlab.arturbosch.detekt.api.Config
import io.gitlab.arturbosch.detekt.test.lint
import io.gitlab.arturbosch.detekt.test.yamlConfig
import org.junit.jupiter.api.Assertions.assertEquals
import org.junit.jupiter.api.Assertions.assertTrue
import org.junit.jupiter.api.BeforeEach
import org.junit.jupiter.api.DisplayName
import org.junit.jupiter.api.Test
import org.junit.jupiter.params.ParameterizedTest
import org.junit.jupiter.params.provider.Arguments
import org.junit.jupiter.params.provider.Arguments.arguments
import org.junit.jupiter.params.provider.MethodSource
import java.util.stream.Stream

internal class MozillaBannedPropertyAccessTest {

    private lateinit var config: Config

    @BeforeEach
    fun setup() {
        config = yamlConfig("/config.yml")

    }

    @Test
    internal fun `non compliant property access should warn`() {
        val findings =
            MozillaBannedPropertyAccess(config).lint(
                NONCOMPLIANT_ACCESS.trimIndent()
            )
        assertEquals(1, findings.size)
        assertEquals(DESCR, findings[0].issue.description)
    }

    @DisplayName("compliant ")
    @MethodSource("compliantProvider")
    @ParameterizedTest(name = "{1} should not warn")
    internal fun testCompliantWhen(source: String) {
        val findings =
            MozillaBannedPropertyAccess(config).lint(
                source
            )
        assertTrue(findings.isEmpty())
    }

    companion object {
        @JvmStatic
        fun compliantProvider(): Stream<Arguments> =
            Stream.of(
                arguments(COMPLIANT_ACCESS, "Safe property access")
            )
    }
}

const val NONCOMPLIANT_ACCESS = """
        class Test {
            lateinit var x: String
            class Sample {
                companion object {
                    public var Banned = "true".toBoolean()
                }
            }
            init {
                if (Sample.Banned) {
                    x = "true"
                }
            }
        }
        """

const val COMPLIANT_ACCESS = """
        class Test {
            lateinit var x: String
            class Sample {
                companion object {
                    public var Banned = "true".toBoolean()
                    public var Allowed = "false".toBoolean()

                }
            }
            init {
                if (Sample.Allowed) {
                    x = "true"
                }
            }
        }
        """
