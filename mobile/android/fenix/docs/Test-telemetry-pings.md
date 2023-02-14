Watch a step by step [video](https://user-images.githubusercontent.com/6579541/170517089-7266b93e-7ff8-4ebb-ae01-4f2a7e558c66.mp4)

1. To send data by default. apply this patch: 
``` diff

diff --git a/app/src/main/java/org/mozilla/fenix/FenixApplication.kt b/app/src/main/java/org/mozilla/fenix/FenixApplication.kt

index 4cb11de43..0c6fab136 100644

--- a/app/src/main/java/org/mozilla/fenix/FenixApplication.kt

+++ b/app/src/main/java/org/mozilla/fenix/FenixApplication.kt

@@ -293,9 +293,7 @@ open class FenixApplication : LocaleAwareApplication(), Provider {

     }

 

     private fun startMetricsIfEnabled() {

-        if (settings().isTelemetryEnabled) {

-            components.analytics.metrics.start(MetricServiceType.Data)

-        }

+        components.analytics.metrics.start(MetricServiceType.Data)

 

         if (settings().isMarketingTelemetryEnabled) {

             components.analytics.metrics.start(MetricServiceType.Marketing)

diff --git a/app/src/main/java/org/mozilla/fenix/components/metrics/MetricController.kt b/app/src/main/java/org/mozilla/fenix/components/metrics/MetricController.kt

index c38ebb62d..3ae102d97 100644

--- a/app/src/main/java/org/mozilla/fenix/components/metrics/MetricController.kt

+++ b/app/src/main/java/org/mozilla/fenix/components/metrics/MetricController.kt

@@ -50,7 +50,7 @@ interface MetricController {

             isMarketingDataTelemetryEnabled: () -> Boolean,

             settings: Settings

         ): MetricController {

-            return if (BuildConfig.TELEMETRY) {

+            return if (true) {

                 ReleaseMetricController(

                     services,

                     isDataTelemetryEnabled,

```

2. Trigger your pings.
3. Sends the ping sing this command:
```
adb shell am start -n org.mozilla.fenix.debug/mozilla.telemetry.glean.debug.GleanDebugActivity \
 --ez logPings true \
 --es sendPing metrics \
 --es debugViewTag test-metrics-ping
```
4. See the results on  https://debug-ping-preview.firebaseapp.com/

The parameters `sendPing` can be  `metrics` or `events` depending or your needs, additionally `debugViewTag` can be customize  to your preferred tag `debugViewTag your-metrics-ping`.



