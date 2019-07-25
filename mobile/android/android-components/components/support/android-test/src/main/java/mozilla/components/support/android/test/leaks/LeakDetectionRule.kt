/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.android.test.leaks

import android.app.Application
import androidx.test.core.app.ApplicationProvider
import com.squareup.leakcanary.InstrumentationLeakDetector
import org.junit.rules.MethodRule
import org.junit.runners.model.FrameworkMethod
import org.junit.runners.model.Statement

/**
 * JUnit rule that will install LeakCanary to detect memory leaks happening while the instrumented tests are running.
 *
 * If a leak is found the test will fail and the test report will contain information about the leak.
 *
 * Note that additionally to adding this rule you need to add the following instrumentation argument:
 *
 * ```
 * android {
 *   defaultConfig {
 *     // ...
 *
 *     testInstrumentationRunnerArgument "listener", "com.squareup.leakcanary.FailTestOnLeakRunListener"
 *    }
 *  }
 * ```
 */
class LeakDetectionRule : MethodRule {
    override fun apply(base: Statement, method: FrameworkMethod, target: Any): Statement {
        return object : Statement() {
            override fun evaluate() {
                try {
                    val application = ApplicationProvider.getApplicationContext<Application>()

                    InstrumentationLeakDetector.instrumentationRefWatcher(application)
                        .buildAndInstall()
                } catch (e: UnsupportedOperationException) {
                    // Ignore
                }

                base.evaluate()
            }
        }
    }
}
