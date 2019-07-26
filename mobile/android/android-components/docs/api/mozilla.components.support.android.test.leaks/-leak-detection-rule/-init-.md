[android-components](../../index.md) / [mozilla.components.support.android.test.leaks](../index.md) / [LeakDetectionRule](index.md) / [&lt;init&gt;](./-init-.md)

# &lt;init&gt;

`LeakDetectionRule()`

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

