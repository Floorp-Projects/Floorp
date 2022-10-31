/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.tooling.lint

import com.android.tools.lint.checks.infrastructure.TestFiles.kotlin
import com.android.tools.lint.checks.infrastructure.TestLintTask.lint
import org.junit.Test
import org.junit.runner.RunWith
import org.junit.runners.JUnit4

/**
 * Tests for the [FactCollectDetector] custom lint check.
 */
@RunWith(JUnit4::class)
class FactCollectDetectorTest {

    private val factClassfileStub = kotlin(
        """
        package mozilla.components.support.base.facts

        data class Fact(
            val component: Component,
            val action: Action,
            val item: String,
            val value: String? = null,
            val metadata: Map<String, Any>? = null
        )

        fun Fact.collect() = Facts.collect(this)
        """,
    ).indented()

    @Test
    fun `should report when collect is not invoked on Fact instance`() {
        lint()
            .files(
                kotlin(
                    """
                    package test

                    import mozilla.components.support.base.facts.Fact
                    import mozilla.components.support.base.facts.collect
                    
                    private fun emitAwesomebarFact(
                        action: Action,
                        item: String,
                        value: String? = null,
                        metadata: Map<String, Any>? = null
                    ) {
                        Fact(
                            Component.BROWSER_AWESOMEBAR,
                            action,
                            item,
                            value,
                            metadata
                        )
                    }
                    """,
                ).indented(),
                factClassfileStub,
            )
            .issues(FactCollectDetector.ISSUE_FACT_COLLECT_CALLED)
            .run()
            .expect(
                """
                src/test/test.kt:12: Error: Fact created but not shown: did you forget to call collect() ? [FactCollect]
                    Fact(
                    ~~~~
                1 errors, 0 warnings
                """.trimIndent(),
            )
    }

    @Test
    fun `should pass when collect is invoked on Fact instance`() {
        lint()
            .files(
                kotlin(
                    """
                    package test

                    import mozilla.components.support.base.facts.Fact
                    import mozilla.components.support.base.facts.collect
                    
                    private fun emitAwesomebarFact(
                        action: Action,
                        item: String,
                        value: String? = null,
                        metadata: Map<String, Any>? = null
                    ) {
                        Fact(
                            Component.BROWSER_AWESOMEBAR,
                            action,
                            item,
                            value,
                            metadata
                        ).collect()
                    }
                    """,
                ).indented(),
                factClassfileStub,
            )
            .issues(FactCollectDetector.ISSUE_FACT_COLLECT_CALLED)
            .run()
            .expectClean()
    }

    @Test
    fun `should pass when an instance escapes through a return statement`() {
        lint()
            .files(
                kotlin(
                    """
                    package test

                    import mozilla.components.support.base.facts.Fact
                    import mozilla.components.support.base.facts.collect
                    
                    private fun createFact(
                        action: Action,
                        item: String,
                        value: String? = null,
                        metadata: Map<String, Any>? = null
                    ): Fact {
                        return Fact(
                            Component.BROWSER_AWESOMEBAR,
                            action,
                            item,
                            value,
                            metadata
                        )
                    }
                    """,
                ).indented(),
                factClassfileStub,
            )
            .issues(FactCollectDetector.ISSUE_FACT_COLLECT_CALLED)
            .run()
            .expectClean()
    }

    @Test
    fun `should pass when an instance escapes through a method parameter`() {
        lint()
            .files(
                kotlin(
                    """
                    package test

                    import mozilla.components.support.base.facts.Fact
                    import mozilla.components.support.base.facts.collect
                    
                    private fun createFact(
                        action: Action,
                        item: String,
                        value: String? = null,
                        metadata: Map<String, Any>? = null
                    ) {
                        val fact = Fact(
                            Component.BROWSER_AWESOMEBAR,
                            action,
                            item,
                            value,
                            metadata
                        )
                        method(fact)
                    }

                    private fun method(parameter: Fact) {
                        
                    }
                    """,
                ).indented(),
                factClassfileStub,
            )
            .issues(FactCollectDetector.ISSUE_FACT_COLLECT_CALLED)
            .run()
            .expectClean()
    }

    @Test
    fun `should pass when an instance escapes through a field assignment`() {
        lint()
            .files(
                kotlin(
                    """
                    package test

                    import mozilla.components.support.base.facts.Fact
                    import mozilla.components.support.base.facts.collect

                    class FactSender {
                        private var fact: Fact? = null
                    
                        private fun createFact(
                            action: Action,
                            item: String,
                            value: String? = null,
                            metadata: Map<String, Any>? = null
                        ) {
                            fact = Fact(
                                Component.BROWSER_AWESOMEBAR,
                                action,
                                item,
                                value,
                                metadata
                            )
                        }
                    }
                    """,
                ).indented(),
                factClassfileStub,
            )
            .issues(FactCollectDetector.ISSUE_FACT_COLLECT_CALLED)
            .run()
            .expectClean()
    }
}
