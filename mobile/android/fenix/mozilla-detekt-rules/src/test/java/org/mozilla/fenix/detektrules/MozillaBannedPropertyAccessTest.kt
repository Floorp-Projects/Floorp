/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.detektrules

import io.gitlab.arturbosch.detekt.test.lint
import io.gitlab.arturbosch.detekt.api.YamlConfig
import org.assertj.core.api.Assertions.assertThat
import org.junit.jupiter.api.DisplayName
import org.junit.jupiter.api.Test
import org.junit.jupiter.params.ParameterizedTest
import org.junit.jupiter.params.provider.Arguments
import org.junit.jupiter.params.provider.Arguments.arguments
import org.junit.jupiter.params.provider.MethodSource
import java.util.stream.Stream

internal class MozillaBannedPropertyAccessTest {
    @Test
    internal fun `non compliant property access should warn`() {
        val findings =
            MozillaBannedPropertyAccess(YamlConfig.loadResource(this.javaClass.getResource("/config.yml"))).lint(
                NONCOMPLIANT_ACCESS.trimIndent()
            )
        assertThat(findings).hasSize(1)
        assertThat(findings[0].issue.description).isEqualTo(DESCR)
    }

    @DisplayName("compliant ")
    @MethodSource("compliantProvider")
    @ParameterizedTest(name = "{1} should not warn")
    internal fun testCompliantWhen(source: String) {
        val findings =
            MozillaBannedPropertyAccess(YamlConfig.loadResource(this.javaClass.getResource("/config.yml"))).lint(
                source
            )
        assertThat(findings).isEmpty()
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