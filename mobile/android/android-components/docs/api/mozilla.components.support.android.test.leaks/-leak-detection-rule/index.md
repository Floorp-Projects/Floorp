[android-components](../../index.md) / [mozilla.components.support.android.test.leaks](../index.md) / [LeakDetectionRule](./index.md)

# LeakDetectionRule

`class LeakDetectionRule : MethodRule` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/android-test/src/main/java/mozilla/components/support/android/test/leaks/LeakDetectionRule.kt#L31)

JUnit rule that will install LeakCanary to detect memory leaks happening while the instrumented tests are running.

If a leak is found the test will fail and the test report will contain information about the leak.

Note that additionally to adding this rule you need to add the following instrumentation argument:

```
android {
  defaultConfig {
    // ...

    testInstrumentationRunnerArgument "listener", "com.squareup.leakcanary.FailTestOnLeakRunListener"
   }
 }
```

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `LeakDetectionRule()`<br>JUnit rule that will install LeakCanary to detect memory leaks happening while the instrumented tests are running. |

### Functions

| Name | Summary |
|---|---|
| [apply](apply.md) | `fun apply(base: Statement, method: FrameworkMethod, target: `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`): Statement` |
