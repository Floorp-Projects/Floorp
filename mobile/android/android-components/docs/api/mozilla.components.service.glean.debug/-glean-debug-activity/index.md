[android-components](../../index.md) / [mozilla.components.service.glean.debug](../index.md) / [GleanDebugActivity](./index.md)

# GleanDebugActivity

`class GleanDebugActivity` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/glean/src/main/java/mozilla/components/service/glean/debug/GleanDebugActivity.kt#L22)

Debugging activity exported by Glean to allow easier debugging.
For example, invoking debug mode in the Glean sample application
can be done via adb using the following command:

adb shell am start -n org.mozilla.samples.glean/mozilla.components.service.glean.debug.GleanDebugActivity

See the adb developer docs for more info:
https://developer.android.com/studio/command-line/adb#am

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `GleanDebugActivity()`<br>Debugging activity exported by Glean to allow easier debugging. For example, invoking debug mode in the Glean sample application can be done via adb using the following command: |

### Functions

| Name | Summary |
|---|---|
| [onCreate](on-create.md) | `fun onCreate(savedInstanceState: <ERROR CLASS>?): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |

### Companion Object Properties

| Name | Summary |
|---|---|
| [LOG_PINGS_EXTRA_KEY](-l-o-g_-p-i-n-g-s_-e-x-t-r-a_-k-e-y.md) | `const val LOG_PINGS_EXTRA_KEY: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
| [SEND_PING_EXTRA_KEY](-s-e-n-d_-p-i-n-g_-e-x-t-r-a_-k-e-y.md) | `const val SEND_PING_EXTRA_KEY: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
| [TAG_DEBUG_VIEW_EXTRA_KEY](-t-a-g_-d-e-b-u-g_-v-i-e-w_-e-x-t-r-a_-k-e-y.md) | `const val TAG_DEBUG_VIEW_EXTRA_KEY: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
| [pingTagPattern](ping-tag-pattern.md) | `val pingTagPattern: `[`Regex`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.text/-regex/index.html) |
